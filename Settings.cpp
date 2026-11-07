// Settings.cpp --- the application settings, the whitelist and their dialogs
#include "Typaste.hpp"
#include "App.hpp"
#include "Settings.hpp"
#include "Config.hpp"
#include "Dpi.hpp"
#include "Util.hpp"
#include <algorithm>
#include <cwctype>
#include <vector>

// The whitelist working copy of the dialog
static std::vector<tstring_t> s_editWhitelist;

// Forward declaration
static tstring_t NormalizeProgramName(const tstring_t& str);

//////////////////////////////////////////////////////////////////////////////
// Data

void Settings_LoadDefaults(void)
{
    g_settings.dwDelayToType = DEFAULT_DELAY;
    g_settings.dwDelayToStart = 0;
    g_settings.wHotKey = DEFALUT_HOTKEY;
    g_settings.bControlIME = TRUE;
    g_settings.bRandom = FALSE;
    g_settings.bOnlyWhitelist = FALSE;
    g_settings.whitelist.clear();

    tstring_t strNone = LoadStr(IDS_NONE);
    if (strNone.empty())
        strNone = TEXT("(None)");
    lstrcpyn(g_szNone, strNone.c_str(), _countof(g_szNone));
    g_settings.strSound = g_szNone;
}

BOOL Settings_Load(void)
{
    Settings_LoadDefaults();

    DWORD dwValue = 0;
    if (Config_ReadInt(TEXT("Delay"), &dwValue))
        g_settings.dwDelayToType = dwValue;
    if (Config_ReadInt(TEXT("DelayToStart"), &dwValue))
        g_settings.dwDelayToStart = dwValue;
    if (Config_ReadInt(TEXT("HotKey"), &dwValue))
        g_settings.wHotKey = (WORD)dwValue;
    if (Config_ReadInt(TEXT("ControlIME"), &dwValue))
        g_settings.bControlIME = !!dwValue;
    if (Config_ReadInt(TEXT("Random"), &dwValue))
        g_settings.bRandom = !!dwValue;
    if (Config_ReadInt(TEXT("OnlyWhitelist"), &dwValue))
        g_settings.bOnlyWhitelist = !!dwValue;

    tstring_t strValue;
    if (Config_ReadString(TEXT("Sound"), strValue))
    {
        StrTrim(&strValue[0], TEXT(" \t\r\n"));
        g_settings.strSound = strValue.empty() ? g_szNone : strValue;
    }

    std::vector<tstring_t> list;
    if (Config_ReadWhitelist(list))
    {
        g_settings.whitelist.clear();
        for (size_t i = 0; i < list.size(); ++i)
        {
            tstring_t name = NormalizeProgramName(list[i]);
            if (!name.empty())
                g_settings.whitelist.push_back(name);
        }
    }

    // The language lives in the same store
    if (Config_ReadString(TEXT("Language"), strValue))
        Lang_SetSetting(Lang_GetSettingFromTag(strValue.c_str()));

    return TRUE;
}

BOOL Settings_Save(void)
{
    BOOL bResult = TRUE;

    bResult &= Config_WriteInt(TEXT("SettingsVersion"), SETTINGS_VERSION);
    bResult &= Config_WriteInt(TEXT("Delay"), g_settings.dwDelayToType);
    bResult &= Config_WriteInt(TEXT("DelayToStart"), g_settings.dwDelayToStart);
    bResult &= Config_WriteInt(TEXT("HotKey"), g_settings.wHotKey);
    bResult &= Config_WriteInt(TEXT("ControlIME"), g_settings.bControlIME);
    bResult &= Config_WriteInt(TEXT("Random"), g_settings.bRandom);
    bResult &= Config_WriteInt(TEXT("OnlyWhitelist"), g_settings.bOnlyWhitelist);
    tstring_t strSound = g_settings.strSound;
    if (strSound == g_szNone)
        strSound.clear();
    bResult &= Config_WriteString(TEXT("Sound"), strSound);
    bResult &= Config_WriteWhitelist(g_settings.whitelist);
    bResult &= Config_WriteString(TEXT("Language"), Lang_GetTag(Lang_GetSetting()));

    return bResult;
}

BOOL Settings_Delete(void)
{
    Settings_LoadDefaults();
    Lang_SetSetting(TYPASTE_LANG_AUTO);
    return Config_Delete();
}

//////////////////////////////////////////////////////////////////////////////
// The whitelist

// Get the lower-case base name of the process that owns the window
static tstring_t GetWindowProcessName(HWND hwnd)
{
    tstring_t name;

    DWORD dwPid = 0;
    GetWindowThreadProcessId(hwnd, &dwPid);
    if (!dwPid)
        return name;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
    if (!hProcess)
        return name;

    WCHAR szPath[MAX_PATH * 2] = { 0 };
    DWORD cch = _countof(szPath);
    if (QueryFullProcessImageNameW(hProcess, 0, szPath, &cch))
    {
        const WCHAR *pszBase = PathFindFileNameW(szPath);
        if (pszBase && *pszBase)
        {
            name = pszBase;
            for (size_t i = 0; i < name.size(); ++i)
                name[i] = (TCHAR)towlower(name[i]);
        }
    }

    CloseHandle(hProcess);
    return name;
}

// Normalize a user supplied program name ("C:\...\Foo.EXE" -> "foo.exe")
static tstring_t NormalizeProgramName(const tstring_t& str)
{
    TCHAR szPath[MAX_PATH * 2];
    lstrcpyn(szPath, str.c_str(), _countof(szPath));

    tstring_t text = szPath;
    size_t a = 0, b = text.size();
    while (a < b && (text[a] == TEXT(' ') || text[a] == TEXT('\t') ||
                     text[a] == TEXT('\r') || text[a] == TEXT('\n')))
        ++a;
    while (b > a && (text[b - 1] == TEXT(' ') || text[b - 1] == TEXT('\t') ||
                     text[b - 1] == TEXT('\r') || text[b - 1] == TEXT('\n')))
        --b;
    text = text.substr(a, b - a);

    const TCHAR *pszBase = PathFindFileName(text.c_str());
    tstring_t name = (pszBase && *pszBase) ? pszBase : text;
    for (size_t i = 0; i < name.size(); ++i)
        name[i] = (TCHAR)towlower(name[i]);
    return name;
}

static BOOL WhitelistContains(const tstring_t& name)
{
    for (size_t i = 0; i < g_settings.whitelist.size(); ++i)
    {
        if (lstrcmpi(g_settings.whitelist[i].c_str(), name.c_str()) == 0)
            return TRUE;
    }
    return FALSE;
}

BOOL Settings_IsForegroundWhitelisted(void)
{
    if (!g_settings.bOnlyWhitelist)
        return TRUE;

    HWND hwndTarget = GetForegroundWindow();
    if (!hwndTarget)
        return FALSE;

    const tstring_t name = GetWindowProcessName(hwndTarget);
    if (name.empty())
        return FALSE;

    return WhitelistContains(name);
}

//////////////////////////////////////////////////////////////////////////////
// The dialogs

// Fill the language combo box
static void Settings_InitLanguage(HWND hwnd)
{
    HWND hCmb = GetDlgItem(hwnd, cmb2);
    if (!hCmb)
        return;

    ComboBox_ResetContent(hCmb);

    tstring_t strAuto = LoadStr(IDS_AUTOLANG);
    if (strAuto.empty())
        strAuto = TEXT("Auto (follow the system)");
    ComboBox_AddString(hCmb, strAuto.c_str());

    // The language names are given in their own languages. \u escapes are used
    // so that this source file stays pure ASCII.
    ComboBox_AddString(hCmb, TEXT("English"));
    ComboBox_AddString(hCmb, TEXT("\u65E5\u672C\u8A9E"));
    ComboBox_AddString(hCmb, TEXT("\u7B80\u4F53\u4E2D\u6587"));
    ComboBox_AddString(hCmb, TEXT("\u7E41\u9AD4\u4E2D\u6587"));

    int setting = Lang_GetSetting();
    if (setting < 0 || setting >= TYPASTE_LANG_COUNT)
        setting = TYPASTE_LANG_AUTO;
    ComboBox_SetCurSel(hCmb, setting);
}

//////////////////////////////////////////////////////////////////////////////
// The whitelist dialog (IDD_WHITELIST)

static void Whitelist_FillList(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, lstPrograms);
    if (!hList)
        return;

    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < s_editWhitelist.size(); ++i)
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)s_editWhitelist[i].c_str());
}

static void Whitelist_AddName(HWND hwnd, const tstring_t& name)
{
    if (name.empty())
        return;
    for (size_t i = 0; i < s_editWhitelist.size(); ++i)
    {
        if (lstrcmpi(s_editWhitelist[i].c_str(), name.c_str()) == 0)
            return; // already there
    }
    s_editWhitelist.push_back(name);
    Whitelist_FillList(hwnd);
}

static void Whitelist_OnAdd(HWND hwnd)
{
    TCHAR szText[MAX_PATH * 2] = TEXT("");
    GetDlgItemText(hwnd, edtName, szText, _countof(szText));
    tstring_t name = NormalizeProgramName(szText);
    if (!name.empty())
    {
        Whitelist_AddName(hwnd, name);
        SetDlgItemText(hwnd, edtName, TEXT(""));
    }
}

static void Whitelist_OnBrowse(HWND hwnd)
{
    tstring_t strFilter = LoadStr(IDS_WHITELIST_FILTER);
    for (size_t i = 0; i < strFilter.size(); ++i)
    {
        if (strFilter[i] == TEXT('|'))
            strFilter[i] = 0;
    }
    if (strFilter.empty() || strFilter[strFilter.size() - 1] != 0)
        strFilter.push_back(0);

    OPENFILENAME ofn;
    TCHAR szPath[MAX_PATH] = TEXT("");
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = strFilter.c_str();
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = _countof(szPath);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = TEXT("exe");
    if (GetOpenFileName(&ofn))
        Whitelist_AddName(hwnd, NormalizeProgramName(szPath));
}

static void Whitelist_OnRemove(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, lstPrograms);
    if (!hList)
        return;

    const int nSel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
    if (nSel < 0 || (size_t)nSel >= s_editWhitelist.size())
        return;

    s_editWhitelist.erase(s_editWhitelist.begin() + nSel);
    Whitelist_FillList(hwnd);
}

static BOOL Whitelist_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    Dpi_InitTracking(hwnd);

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);

    s_editWhitelist = g_settings.whitelist;
    CheckDlgButton(hwnd, chkWhitelist, g_settings.bOnlyWhitelist ? BST_CHECKED : BST_UNCHECKED);
    Whitelist_FillList(hwnd);
    SetForegroundWindow(hwnd);
    return TRUE;
}

static void Whitelist_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    switch (id)
    {
    case IDOK:
        g_settings.whitelist = s_editWhitelist;
        g_settings.bOnlyWhitelist = (IsDlgButtonChecked(hwnd, chkWhitelist) == BST_CHECKED);
        Settings_Save();
        EndDialog(hwnd, IDOK);
        break;
    case IDCANCEL:
        EndDialog(hwnd, IDCANCEL);
        break;
    case btnAdd:
        Whitelist_OnAdd(hwnd);
        break;
    case btnBrowseExe:
        Whitelist_OnBrowse(hwnd);
        break;
    case btnRemove:
        Whitelist_OnRemove(hwnd);
        break;
    }
}

static INT_PTR CALLBACK
WhitelistDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DPICHANGED)
        return Dpi_HandleChanged(hwnd, wParam, lParam);

    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Whitelist_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Whitelist_OnCommand);
    default:
        break;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// The settings dialog (IDD_SETTINGS)

// The range of the speed slider (milliseconds per character)
#define SPEED_SLIDER_MAX 200

static BOOL Settings_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    Dpi_InitTracking(hwnd);

    // Set the main icon
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);

    // Set the delay values
    SetDlgItemInt(hwnd, edt1, g_settings.dwDelayToType, FALSE);
    SetDlgItemInt(hwnd, edt3, g_settings.dwDelayToStart, FALSE);

    // Set hot-key value
    SendDlgItemMessage(hwnd, edt2, HKM_SETRULES, HKCOMB_A | HKCOMB_NONE | HKCOMB_S, 0);
    SendDlgItemMessage(hwnd, edt2, HKM_SETHOTKEY, g_settings.wHotKey, 0);

    // Set value range
    SendDlgItemMessage(hwnd, scr1, UDM_SETRANGE, 0, MAKELONG(10000, 0));
    SendDlgItemMessage(hwnd, scr2, UDM_SETRANGE, 0, MAKELONG(10000, 0));

    // The speed slider
    SendDlgItemMessage(hwnd, sld1, TBM_SETRANGE, TRUE, MAKELONG(0, SPEED_SLIDER_MAX));
    SendDlgItemMessage(hwnd, sld1, TBM_SETTICFREQ, 50, 0);
    SendDlgItemMessage(hwnd, sld1, TBM_SETPOS, TRUE,
                       (LPARAM)(g_settings.dwDelayToType > SPEED_SLIDER_MAX ? SPEED_SLIDER_MAX : g_settings.dwDelayToType));

    // Build a wildcard pattern for WAV files
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("*.wav"));

    // Load "(None)"
    tstring_t strNone = LoadStr(IDS_NONE);
    if (strNone.empty())
        strNone = TEXT("(None)");
    lstrcpyn(g_szNone, strNone.c_str(), _countof(g_szNone));

    // Add "(None)" to the combo box
    HWND hCmb1 = GetDlgItem(hwnd, cmb1);
    ComboBox_ResetContent(hCmb1);
    ComboBox_AddString(hCmb1, g_szNone);

    // Collect the sound files (sorted)
    std::vector<tstring_t> files;
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(szPath, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            files.push_back(find.cFileName);
        } while (FindNextFile(hFind, &find));
        FindClose(hFind);
    }
    std::sort(files.begin(), files.end());
    for (size_t i = 0; i < files.size(); ++i)
        ComboBox_AddString(hCmb1, files[i].c_str());

    if (ComboBox_FindStringExact(hCmb1, -1, g_settings.strSound.c_str()) == CB_ERR)
        ComboBox_AddString(hCmb1, g_settings.strSound.c_str());
    ComboBox_SetText(hCmb1, g_settings.strSound.c_str());

    // Control IME?
    CheckDlgButton(hwnd, chx1, g_settings.bControlIME ? BST_CHECKED : BST_UNCHECKED);

    // Random interval?
    CheckDlgButton(hwnd, chx2, g_settings.bRandom ? BST_CHECKED : BST_UNCHECKED);

    // The language choice
    Settings_InitLanguage(hwnd);

    SetForegroundWindow(hwnd);
    return TRUE;
}

// WM_HSCROLL: the speed slider drives the delay edit box
static void Settings_OnHScroll(HWND hwnd, int nCode, HWND hwndCtl)
{
    UNREFERENCED_PARAMETER(nCode);

    if (hwndCtl != GetDlgItem(hwnd, sld1))
        return;

    const int nPos = (int)SendMessage(hwndCtl, TBM_GETPOS, 0, 0);
    SetDlgItemInt(hwnd, edt1, (UINT)nPos, FALSE);
}

static void Settings_OnReset(HWND hwnd)
{
    tstring_t strConfirm = LoadStr(IDS_RESET_CONFIRM);
    if (strConfirm.empty())
        strConfirm = TEXT("Reset all settings to the defaults?");

    if (MessageBox(hwnd, strConfirm.c_str(), NULL,
                   MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }

    Settings_Delete();
    Settings_Load();
    Settings_Save();

    // The main window is re-created by WinMain with the default language
    EndDialog(hwnd, IDOK);
}

static void Settings_OnOK(HWND hwnd)
{
    g_settings.dwDelayToType = GetDlgItemInt(hwnd, edt1, NULL, FALSE);
    g_settings.dwDelayToStart = GetDlgItemInt(hwnd, edt3, NULL, FALSE);
    g_settings.wHotKey = (WORD)SendDlgItemMessage(hwnd, edt2, HKM_GETHOTKEY, 0, 0);
    g_settings.bControlIME = (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED);
    g_settings.bRandom = (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED);

    TCHAR szText[MAX_PATH];
    GetDlgItemText(hwnd, cmb1, szText, _countof(szText));
    StrTrim(szText, TEXT(" \t\r\n"));
    g_settings.strSound = szText;

    // The language choice. The main window is re-created by WinMain after this
    // dialog closes, so the new language takes effect immediately.
    int nLang = ComboBox_GetCurSel(GetDlgItem(hwnd, cmb2));
    if (nLang < 0 || nLang >= TYPASTE_LANG_COUNT)
        nLang = TYPASTE_LANG_AUTO;
    if (nLang != Lang_GetSetting())
        Lang_SetSetting(nLang);

    Settings_Save();
    EndDialog(hwnd, IDOK);
}

static void Settings_OnBrowse(HWND hwnd)
{
    tstring_t strTitle = LoadStr(IDS_CHOOSESOUNDFILE);

    tstring_t strFilter = LoadStr(IDS_SOUNDFILTER);
    for (size_t i = 0; i < strFilter.size(); ++i)
    {
        if (strFilter[i] == TEXT('|'))
            strFilter[i] = 0;
    }
    if (strFilter.empty() || strFilter[strFilter.size() - 1] != 0)
        strFilter.push_back(0);

    OPENFILENAME ofn;
    TCHAR szPath[MAX_PATH] = TEXT("");
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = strFilter.c_str();
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = _countof(szPath);
    ofn.lpstrTitle = strTitle.empty() ? NULL : strTitle.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                OFN_HIDEREADONLY | OFN_LONGNAMES;
    ofn.lpstrDefExt = TEXT("wav");
    if (GetOpenFileName(&ofn))
        SetDlgItemText(hwnd, cmb1, szPath);
}

static void Settings_OnPlay(HWND hwnd)
{
    TCHAR szPath[MAX_PATH];
    GetDlgItemText(hwnd, cmb1, szPath, _countof(szPath));
    PlaySound(szPath, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
}

static void Settings_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    switch (id)
    {
    case IDOK:
        Settings_OnOK(hwnd);
        break;
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case psh2:
        Settings_OnBrowse(hwnd);
        break;
    case psh3:
        Settings_OnPlay(hwnd);
        break;
    case btnReset:
        Settings_OnReset(hwnd);
        break;
    case btnWhitelist:
        Lang_DialogBox(g_hInst, MAKEINTRESOURCE(IDD_WHITELIST), hwnd, WhitelistDialogProc, 0);
        break;
    }
}

static INT_PTR CALLBACK
SettingsDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DPICHANGED)
        return Dpi_HandleChanged(hwnd, wParam, lParam);

    if (uMsg == WM_HSCROLL)
    {
        Settings_OnHScroll(hwnd, (int)LOWORD(wParam), (HWND)lParam);
        return 0;
    }

    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Settings_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Settings_OnCommand);
    default:
        break;
    }
    return 0;
}

void Settings_Show(HWND hwndParent)
{
    Lang_DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwndParent, SettingsDialogProc, 0);
}
