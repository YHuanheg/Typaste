// Typaste.cpp --- Typing + Paste
// Copyright (C) 2019-2026 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
#include "Typaste.hpp"
#include <vector>
#include <algorithm>

// The default delay value (in milliseconds)
#define DEFAULT_DELAY 20

// The default hot-key value
#define DEFALUT_HOTKEY MAKEWORD('V', HOTKEYF_CONTROL) // Ctrl+V

// The hot-key ID. It must be a WORD
#define HOTKEY_ID 0xDEDD

// The window property that marks the main window of this process
#define MAINWINDOW_PROP TEXT("Typaste.MainWindow")

// The name of the single instance mutex
#define SINGLE_INSTANCE_NAME L"Local\\Typaste.SingleInstance"

// The registry key of the settings
#define APP_REGKEY TYP_REGKEY

// The version of the settings layout
#define SETTINGS_VERSION 2

// The maximum number of characters to type at once
#define MAX_TYPING_CHARS 100000

// The notification that the typing thread has finished
#define WM_APP_TYPING_DONE (WM_APP + 1)

// The global variables
HINSTANCE   g_hInst             = NULL;            // The instance handle of this application
HWND        g_hwndMain          = NULL;            // The main window handle
DWORD       g_dwDelayToType     = DEFAULT_DELAY;   // The delay to type (in milliseconds)
DWORD       g_dwDelayToStart    = 0;               // The delay to start (in milliseconds)
WORD        g_wHotKey           = DEFALUT_HOTKEY;  // The hot-key value
HICON       g_hIcon             = NULL;            // The application main icon (normal)
HICON       g_hIconSm           = NULL;            // The application main icon (small)
INT         g_nExitCode         = IDCANCEL;        // The exit code
BOOL        g_bControlIME       = TRUE;            // Does the application control IME?
BOOL        g_bRandom           = FALSE;           // Random interval?
TCHAR       g_szNone[64]        = TEXT("(None)");  // "(None)"
tstring_t   g_strSound;                            // The sound filename

volatile LONG g_bAbortTyping   = 0;                // Stop the typing thread now?
static volatile LONG s_bTyping = 0;                // Is the typing thread running?
static HANDLE s_hTypingThread  = NULL;             // The typing thread
static HANDLE s_hSingleInstance = NULL;            // The single instance mutex

// Load a localized string from the resource of the current language
static tstring_t LoadStr(UINT id)
{
    std::wstring str = Lang_LoadString(id);
#ifdef UNICODE
    return str;
#else
    std::string ansi;
    ansi.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
        ansi.push_back((char)str[i]);
    return ansi;
#endif
}

// Format a message that takes one string argument ("%s")
static tstring_t FormatStr(const tstring_t& fmt, LPCTSTR pszArg)
{
    TCHAR szText[1024] = TEXT("");
    if (fmt.empty())
        return szText;
    wsprintf(szText, fmt.c_str(), pszArg);
    return szText;
}

// Build a readable hot-key name like "Ctrl + V"
static tstring_t FormatHotKey(WORD wHotKey)
{
    UINT vk = LOBYTE(wHotKey), flags = HIBYTE(wHotKey);
    tstring_t str;

    if (flags & HOTKEYF_CONTROL)
        str += TEXT("Ctrl + ");
    if (flags & HOTKEYF_ALT)
        str += TEXT("Alt + ");
    if (flags & HOTKEYF_SHIFT)
        str += TEXT("Shift + ");

    TCHAR szName[80] = TEXT("");
    UINT nScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    switch (vk)
    {
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        nScan |= 0x100; // extended key
        break;
    }
    if (GetKeyNameText(nScan << 16, szName, _countof(szName)) == 0)
        wsprintf(szName, TEXT("0x%02X"), vk);

    str += szName;
    return str;
}

// A bigger, bold font for the dialog titles
static HFONT s_hFontTitle = NULL;

static void SetupTitleFont(HWND hwnd)
{
    if (!s_hFontTitle)
    {
        HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        LOGFONT lf;
        ZeroMemory(&lf, sizeof(lf));
        if (hFont && GetObject(hFont, sizeof(lf), &lf))
        {
            lf.lfHeight = lf.lfHeight * 14 / 10;
            lf.lfWeight = FW_BOLD;
            s_hFontTitle = CreateFontIndirect(&lf);
        }
    }
    if (s_hFontTitle)
        SendDlgItemMessage(hwnd, txtTitle, WM_SETFONT, (WPARAM)s_hFontTitle, TRUE);
}

// Update the hot-key line of the main dialog
static void UpdateHotKeyText(HWND hwnd)
{
    tstring_t strText = FormatStr(LoadStr(IDS_HOTKEY_FMT), FormatHotKey(g_wHotKey).c_str());
    SetDlgItemText(hwnd, txtHotkey, strText.c_str());
}

// Update the status line of the main dialog
static void UpdateStatusText(HWND hwnd)
{
    tstring_t strText = LoadStr(s_bTyping ? IDS_STATUS_TYPING : IDS_STATUS_READY);
    if (strText.empty())
        strText = s_bTyping ? TEXT("Typing...") : TEXT("Ready");
    SetDlgItemText(hwnd, txtStatus, strText.c_str());
}

// Register a hot-key
BOOL MyRegisterHotKey(HWND hwnd)
{
    UINT vk = LOBYTE(g_wHotKey), flags = HIBYTE(g_wHotKey);

    // Convert HOTKEYF_* flags to MOD_* flags
    UINT nMod = 0;
    if (flags & HOTKEYF_ALT)
        nMod |= MOD_ALT;
    if (flags & HOTKEYF_CONTROL)
        nMod |= MOD_CONTROL;
    if (flags & HOTKEYF_SHIFT)
        nMod |= MOD_SHIFT;

    // Un-register now
    UnregisterHotKey(hwnd, HOTKEY_ID);

    // Register the hot-key
    if (RegisterHotKey(hwnd, HOTKEY_ID, nMod, vk))
        return TRUE; // Success!

    // Failed. Show error message
    tstring_t strFail = LoadStr(IDS_FAILHOTKEY);
    if (strFail.empty())
        strFail = TEXT("Failed to register the hot-key.");
    MessageBox(hwnd, strFail.c_str(), NULL, MB_ICONERROR);

    return FALSE; // Failed
}

// Fill the language combo box
static void Settings_InitLanguage(HWND hwnd)
{
    HWND hCmb = GetDlgItem(hwnd, cmb2);
    if (!hCmb)
        return;

    ComboBox_ResetContent(hCmb);

    // "Follow the system"
    tstring_t strAuto = LoadStr(IDS_AUTOLANG);
    if (strAuto.empty())
        strAuto = TEXT("Auto (follow the system)");
    ComboBox_AddString(hCmb, strAuto.c_str());

    // The language names are given in their own languages. \u escapes are used
    // so that this source file stays pure ASCII.
    ComboBox_AddString(hCmb, TEXT("English"));                      // English
    ComboBox_AddString(hCmb, TEXT("\u65E5\u672C\u8A9E"));            // Japanese
    ComboBox_AddString(hCmb, TEXT("\u7B80\u4F53\u4E2D\u6587"));      // Simplified Chinese
    ComboBox_AddString(hCmb, TEXT("\u7E41\u9AD4\u4E2D\u6587"));      // Traditional Chinese

    int setting = Lang_GetSetting();
    if (setting < 0 || setting >= TYPASTE_LANG_COUNT)
        setting = TYPASTE_LANG_AUTO;
    ComboBox_SetCurSel(hCmb, setting);
}

// IDD_ABOUT
INT_PTR CALLBACK AboutDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);
        SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIcon);
        SetupTitleFont(hwnd);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hwnd, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// WM_INITDIALOG
BOOL Settings_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    // Set the main icon
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);

    // Set the delay values
    SetDlgItemInt(hwnd, edt1, g_dwDelayToType, FALSE);
    SetDlgItemInt(hwnd, edt3, g_dwDelayToStart, FALSE);

    // Set hot-key value
    SendDlgItemMessage(hwnd, edt2, HKM_SETRULES, HKCOMB_A | HKCOMB_NONE | HKCOMB_S, 0);
    SendDlgItemMessage(hwnd, edt2, HKM_SETHOTKEY, g_wHotKey, 0);

    // Set value range
    SendDlgItemMessage(hwnd, scr1, UDM_SETRANGE, 0, MAKELONG(10000, 0));
    SendDlgItemMessage(hwnd, scr2, UDM_SETRANGE, 0, MAKELONG(10000, 0));

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

    // Collect the sound files (sorted, unlike the raw FindFirstFile order)
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

    // Add g_strSound if any
    if (ComboBox_FindStringExact(hCmb1, -1, g_strSound.c_str()) == CB_ERR)
        ComboBox_AddString(hCmb1, g_strSound.c_str());
    // Set g_strSound
    ComboBox_SetText(hCmb1, g_strSound.c_str());

    // Control IME?
    CheckDlgButton(hwnd, chx1, g_bControlIME ? BST_CHECKED : BST_UNCHECKED);

    // Random interval?
    CheckDlgButton(hwnd, chx2, g_bRandom ? BST_CHECKED : BST_UNCHECKED);

    // The language choice
    Settings_InitLanguage(hwnd);

    // Set foreground
    SetForegroundWindow(hwnd);

    return TRUE; // Default focus
}

// Erase settings
BOOL Settings_Delete(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    LONG nError = RegDeleteKey(HKEY_CURRENT_USER, APP_REGKEY);
    return (ERROR_SUCCESS == nError);
}

// Load settings
BOOL Settings_Load(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    // Load the default values
    g_dwDelayToType = DEFAULT_DELAY;
    g_dwDelayToStart = 0;
    g_wHotKey = DEFALUT_HOTKEY;
    g_bControlIME = TRUE;
    g_bRandom = FALSE;

    tstring_t strNone = LoadStr(IDS_NONE);
    if (strNone.empty())
        strNone = TEXT("(None)");
    lstrcpyn(g_szNone, strNone.c_str(), _countof(g_szNone));
    g_strSound = g_szNone;

    // Open the application registry key
    HKEY hKey = NULL;
    RegOpenKeyEx(HKEY_CURRENT_USER, APP_REGKEY, 0, KEY_READ, &hKey);
    if (!hKey)
        return FALSE;

    //
    // Set the registry values
    //
    DWORD dwData, cbData;
    TCHAR szText[MAX_PATH];
    LONG error;

    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("Delay"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        g_dwDelayToType = dwData;
    }
    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("DelayToStart"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        g_dwDelayToStart = dwData;
    }

    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("HotKey"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        g_wHotKey = (WORD)dwData;
    }

    cbData = sizeof(szText);
    error = RegQueryValueEx(hKey, TEXT("Sound"), NULL, NULL, (LPBYTE)szText, &cbData);
    if (!error)
    {
        szText[_countof(szText) - 1] = 0;
        StrTrim(szText, TEXT(" \t\r\n"));
        g_strSound = szText;
        if (g_strSound.empty())
            g_strSound = g_szNone;
    }

    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("ControlIME"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        g_bControlIME = !!dwData;
    }

    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("Random"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        g_bRandom = !!dwData;
    }

    // Close the registry key
    RegCloseKey(hKey);
    return TRUE;
}

// Save settings
BOOL Settings_Save(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    // Create or open the application registry key
    HKEY hAppKey = NULL;
    RegCreateKeyEx(HKEY_CURRENT_USER, APP_REGKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hAppKey, NULL);
    if (!hAppKey)
        return FALSE;

    //
    // Set the registry values
    //
    DWORD dwVersion = SETTINGS_VERSION;
    RegSetValueEx(hAppKey, TEXT("SettingsVersion"), 0, REG_DWORD, (const BYTE *)&dwVersion, sizeof(dwVersion));

    RegSetValueEx(hAppKey, TEXT("Delay"), 0, REG_DWORD, (const BYTE *)&g_dwDelayToType, sizeof(DWORD));
    RegSetValueEx(hAppKey, TEXT("DelayToStart"), 0, REG_DWORD, (const BYTE *)&g_dwDelayToStart, sizeof(DWORD));
    RegSetValueEx(hAppKey, TEXT("ControlIME"), 0, REG_DWORD, (const BYTE *)&g_bControlIME, sizeof(g_bControlIME));
    RegSetValueEx(hAppKey, TEXT("Random"), 0, REG_DWORD, (const BYTE *)&g_bRandom, sizeof(g_bRandom));

    DWORD dwData = g_wHotKey;
    RegSetValueEx(hAppKey, TEXT("HotKey"), 0, REG_DWORD, (const BYTE *)&dwData, sizeof(dwData));

    DWORD cbData = (DWORD)((g_strSound.size() + 1) * sizeof(TCHAR));
    RegSetValueEx(hAppKey, TEXT("Sound"), 0, REG_SZ, (const BYTE *)g_strSound.c_str(), cbData);

    // The language choice (a BCP-47 tag such as "zh-CN", or "auto")
    const wchar_t *pszLang = Lang_GetTag(Lang_GetSetting());
    RegSetValueExW(hAppKey, L"Language", 0, REG_SZ, (const BYTE *)pszLang,
                   (DWORD)((wcslen(pszLang) + 1) * sizeof(wchar_t)));

    // Close the registry key
    RegCloseKey(hAppKey);

    return TRUE;
}

// IDD_SETTINGS WM_COMMAND psh5
void Settings_OnReset(HWND hwnd)
{
    tstring_t strConfirm = LoadStr(IDS_RESET_CONFIRM);
    if (strConfirm.empty())
        strConfirm = TEXT("Reset all settings to the defaults?");

    if (MessageBox(hwnd, strConfirm.c_str(), NULL,
                   MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }

    // Forget the stored settings and fall back to the defaults
    Settings_Delete(hwnd);
    Lang_SetSetting(TYPASTE_LANG_AUTO);
    Settings_Load(hwnd);
    Settings_Save(hwnd);

    // The main window is re-created by WinMain with the default language
    EndDialog(hwnd, IDOK);
}

// IDD_SETTINGS WM_COMMAND IDOK
void Settings_OnOK(HWND hwnd)
{
    //
    // Update settings from dialog
    //
    g_dwDelayToType = GetDlgItemInt(hwnd, edt1, NULL, FALSE);
    g_dwDelayToStart = GetDlgItemInt(hwnd, edt3, NULL, FALSE);
    g_wHotKey = (WORD)SendDlgItemMessage(hwnd, edt2, HKM_GETHOTKEY, 0, 0);
    g_bControlIME = (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED);
    g_bRandom = (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED);

    TCHAR szText[MAX_PATH];
    GetDlgItemText(hwnd, cmb1, szText, _countof(szText));
    StrTrim(szText, TEXT(" \t\r\n"));
    g_strSound = szText;

    // The language choice. The main window is re-created by WinMain after this
    // dialog closes, so the new language takes effect immediately.
    int nLang = ComboBox_GetCurSel(GetDlgItem(hwnd, cmb2));
    if (nLang < 0 || nLang >= TYPASTE_LANG_COUNT)
        nLang = TYPASTE_LANG_AUTO;
    if (nLang != Lang_GetSetting())
        Lang_SetSetting(nLang);

    // Save settings
    Settings_Save(hwnd);

    // End the settings dialog
    EndDialog(hwnd, IDOK);
}

// IDD_SETTINGS WM_COMMAND psh2
void Settings_OnBrowse(HWND hwnd)
{
    // Load title
    tstring_t strTitle = LoadStr(IDS_CHOOSESOUNDFILE);

    // Build the filter string
    tstring_t strFilter = LoadStr(IDS_SOUNDFILTER);
    for (size_t i = 0; i < strFilter.size(); ++i)
    {
        if (strFilter[i] == TEXT('|'))
            strFilter[i] = 0;
    }
    if (strFilter.empty() || strFilter[strFilter.size() - 1] != 0)
        strFilter.push_back(0);

    // Open the "Open file" dialog and wait for the user
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
    {
        // Set the path
        SetDlgItemText(hwnd, cmb1, szPath);
    }
}

// IDD_SETTINGS WM_COMMAND psh3
void Settings_OnPlay(HWND hwnd)
{
    TCHAR szPath[MAX_PATH];
    GetDlgItemText(hwnd, cmb1, szPath, _countof(szPath));
    PlaySound(szPath, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
}

// IDD_SETTINGS WM_COMMAND
void Settings_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
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
    }
}

// The dialog procedure of settings dialog IDD_SETTINGS
INT_PTR CALLBACK
SettingsDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Settings_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Settings_OnCommand);
    default:
        break;
    }
    return 0;
}

// Find the main window of this process regardless of the language
static BOOL CALLBACK EnumMainWindows(HWND hwnd, LPARAM lParam)
{
    if (GetProp(hwnd, MAINWINDOW_PROP))
    {
        *(HWND *)lParam = hwnd;
        return FALSE; // stop
    }
    return TRUE;
}

static HWND FindMainWindow(void)
{
    HWND hwndFound = NULL;
    EnumWindows(EnumMainWindows, (LPARAM)&hwndFound);
    return hwndFound;
}

// The data that is handed over to the typing thread
struct TYPE_JOB
{
    std::wstring text;
    std::wstring sound;
    DWORD dwDelayToType;
    DWORD dwDelayToStart;
    BOOL bRandom;
    BOOL bControlIME;
    HWND hwndIme;
    BOOL bImeOn;
};

#ifndef IMC_GETOPENSTATUS
    #define IMC_GETOPENSTATUS 0x0005
#endif
#ifndef IMC_SETOPENSTATUS
    #define IMC_SETOPENSTATUS 0x0006
#endif

// The typing thread: it keeps the UI responsive while the text is being typed
static DWORD WINAPI TypingThreadProc(LPVOID lpParam)
{
    TYPE_JOB *pJob = (TYPE_JOB *)lpParam;

    // Wait for start
    MySleep(pJob->dwDelayToStart, pJob->bRandom);

    // Wait for release of modifier keys
    WaitModifierRelease(pJob->dwDelayToType, pJob->bRandom);

    // Close IME if necessary
    if (pJob->bControlIME && pJob->hwndIme && pJob->bImeOn)
    {
        SendMessage(pJob->hwndIme, WM_IME_CONTROL, IMC_SETOPENSTATUS, FALSE);
        Sleep(300);
    }

    // Start auto typing
    AutoType(pJob->text.c_str(), pJob->dwDelayToType,
             pJob->sound.empty() ? NULL : pJob->sound.c_str(), pJob->bRandom);

    // Re-open IME if necessary
    if (pJob->bControlIME && pJob->hwndIme && pJob->bImeOn)
    {
        SendMessage(pJob->hwndIme, WM_IME_CONTROL, IMC_SETOPENSTATUS, TRUE);
        Sleep(300);
    }

    delete pJob;

    InterlockedExchange(&s_bTyping, 0);
    if (g_hwndMain)
        PostMessage(g_hwndMain, WM_APP_TYPING_DONE, 0, 0);

    return 0;
}

// IDD_MAIN WM_INITDIALOG
BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    // Remember the main window handle
    g_hwndMain = hwnd;

    // Mark this window so that the next instance can find it
    SetProp(hwnd, MAINWINDOW_PROP, (HANDLE)1);

    // Load settings
    Settings_Load(hwnd);

    // Register the hot-key; if that fails, open the settings right away
    BOOL bHotKey = MyRegisterHotKey(hwnd);

    // Set the icons to the dialog
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);
    SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIcon);

    SetupTitleFont(hwnd);


    // Show the current state
    UpdateHotKeyText(hwnd);
    UpdateStatusText(hwnd);

    if (!bHotKey)
        PostMessage(hwnd, WM_COMMAND, psh1, 0);

    return TRUE; // Default focus
}

// IDD_MAIN WM_COMMAND psh2
void OnOpenReadMe(HWND hwnd)
{
    // Load ReadMe filename
    tstring_t strReadMe = LoadStr(IDS_README);
    if (strReadMe.empty())
        strReadMe = TEXT("\\README.txt");

    // Build a path for "ReadMe.txt"
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, strReadMe.c_str());

    // Open "ReadMe.txt"
    ShellExecute(hwnd, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
}

// IDD_MAIN WM_COMMAND
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    // This hwnd is a modeless dialog. We use DestroyWindow instead of EndDialog for this hwnd.
    switch (id)
    {
    case IDOK:
        g_nExitCode = id;
        DestroyWindow(hwnd);
        break;
    case IDCANCEL:
        g_nExitCode = id;
        DestroyWindow(hwnd);
        break;
    case psh1:
        g_nExitCode = id;
        DestroyWindow(hwnd);
        break;
    case psh2:
        OnOpenReadMe(hwnd);
        break;
    case btnAbout:
        Lang_DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDialogProc, 0);
        break;
    }
}

// Normal Ctrl+V or something
void NormalCtrlV(HWND hwnd, BOOL bRandom)
{
    // Un-register the hot-key
    UnregisterHotKey(hwnd, HOTKEY_ID);

    // Wait for release of modifier keys
    WaitModifierRelease(g_dwDelayToType, bRandom);

    // Press Ctrl+V or something
    CtrlV(g_dwDelayToType, bRandom);

    // Re-register the hot-key
    MyRegisterHotKey(hwnd);
}

// IDD_MAIN WM_HOTKEY
void OnHotKey(HWND hwnd, int idHotKey, UINT fuModifiers, UINT vk)
{
    UNREFERENCED_PARAMETER(fuModifiers);
    UNREFERENCED_PARAMETER(vk);

    // Sanity check
    if (idHotKey != HOTKEY_ID)
        return;

    // Do not start a second typing thread
    if (s_bTyping)
    {
        tstring_t strBusy = LoadStr(IDS_BUSY);
        if (!strBusy.empty())
            MessageBox(hwnd, strBusy.c_str(), NULL, MB_ICONINFORMATION);
        return;
    }

    // Is the clipboard text available?
    if (!IsClipboardFormatAvailable(CF_GENERICTEXT) || !OpenClipboard(hwnd))
    {
        // Text data in clipboard is not available.
        NormalCtrlV(hwnd, g_bRandom);
        return;
    }

    // Get the clipbard text
    LPTSTR pszClone = NULL;
    if (HGLOBAL hText = GetClipboardData(CF_GENERICTEXT))
    {
        if (LPTSTR pszText = (LPTSTR)GlobalLock(hText))
        {
            pszClone = _tcsdup(pszText);
            GlobalUnlock(hText);
        }
    }

    // Close the clipboard
    CloseClipboard();

    if (!pszClone)
    {
        // Text data in clipboard is not available.
        NormalCtrlV(hwnd, g_bRandom);
        return;
    }

    // Refuse an absurdly long text instead of blocking for minutes
    size_t cch = _tcslen(pszClone);
    if (cch > MAX_TYPING_CHARS)
    {
        TCHAR szText[512];
        wsprintf(szText, LoadStr(IDS_TEXT_TOO_LONG).c_str(), (int)MAX_TYPING_CHARS);
        MessageBox(hwnd, szText, NULL, MB_ICONWARNING);
        free(pszClone);
        return;
    }

    // Prepare for the sound file
    TCHAR szSoundFile[MAX_PATH];
    LPCTSTR pszSound = g_strSound.c_str();
    if (g_strSound == g_szNone)
    {
        szSoundFile[0] = 0;
    }
    else if (PathIsRelative(pszSound))
    {
        GetModuleFileName(NULL, szSoundFile, _countof(szSoundFile));
        PathRemoveFileSpec(szSoundFile);
        PathAppend(szSoundFile, pszSound);
    }
    else
    {
        lstrcpyn(szSoundFile, pszSound, _countof(szSoundFile));
    }

    // This is the target window
    HWND hwndTarget = GetForegroundWindow();

    // Is there an IME window?
    HWND hwndIme = ImmGetDefaultIMEWnd(hwndTarget);

    // Is the IME ON?
    BOOL bImeOn = (BOOL)SendMessage(hwndIme, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);

    // Build the job for the typing thread
    TYPE_JOB *pJob = new TYPE_JOB();
    pJob->text = pszClone;
    pJob->sound = szSoundFile;
    pJob->dwDelayToType = g_dwDelayToType;
    pJob->dwDelayToStart = g_dwDelayToStart;
    pJob->bRandom = g_bRandom;
    pJob->bControlIME = g_bControlIME;
    pJob->hwndIme = hwndIme;
    pJob->bImeOn = bImeOn;

    free(pszClone);

    // Start the typing thread
    InterlockedExchange(&g_bAbortTyping, 0);
    InterlockedExchange(&s_bTyping, 1);

    if (s_hTypingThread)
    {
        CloseHandle(s_hTypingThread);
        s_hTypingThread = NULL;
    }

    s_hTypingThread = CreateThread(NULL, 0, TypingThreadProc, pJob, 0, NULL);
    if (!s_hTypingThread)
    {
        InterlockedExchange(&s_bTyping, 0);
        delete pJob;
        NormalCtrlV(hwnd, g_bRandom);
        return;
    }

    UpdateStatusText(hwnd);
}

// IDD_MAIN WM_DESTROY
void OnDestroy(HWND hwnd)
{
    Settings_Save(hwnd);

    // Stop the typing thread
    InterlockedExchange(&g_bAbortTyping, 1);
    if (s_hTypingThread)
    {
        WaitForSingleObject(s_hTypingThread, 3000);
        CloseHandle(s_hTypingThread);
        s_hTypingThread = NULL;
    }

    // Remove the hot-key and the window mark
    UnregisterHotKey(hwnd, HOTKEY_ID);
    RemoveProp(hwnd, MAINWINDOW_PROP);

    if (g_hwndMain == hwnd)
        g_hwndMain = NULL;

    PostQuitMessage(0);
}

#ifndef HANDLE_WM_HOTKEY
    #define HANDLE_WM_HOTKEY(hwnd,wParam,lParam,fn) \
        ((fn)((hwnd),(int)(wParam),(UINT)LOWORD(lParam),(UINT)HIWORD(lParam)),(LRESULT)0)
#endif

// The main dialog IDD_MAIN procedure
INT_PTR CALLBACK
MainDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_APP_TYPING_DONE)
    {
        if (s_hTypingThread)
        {
            CloseHandle(s_hTypingThread);
            s_hTypingThread = NULL;
        }
        UpdateStatusText(hwnd);
        return 0;
    }

    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_HOTKEY, OnHotKey);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    default:
        break;
    }
    return 0;
}

// The main function of Win32 application
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Remember the application instance
    g_hInst = hInstance;

    // Random seed
    std::srand((unsigned int)::GetTickCount());

    // Choose the language before any resource is loaded
    Lang_Init(hInstance);

    // Only one instance per user session
    s_hSingleInstance = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_NAME);
    if (s_hSingleInstance && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Already running. Move focus to the existing window
        if (HWND hwndMain = FindMainWindow())
            SetForegroundWindow(hwndMain);
        CloseHandle(s_hSingleInstance);
        return 0;
    }

    // Initialize common controls
    InitCommonControls();

    // Load main icons
    g_hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    g_hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

    // The loop
    for (;;)
    {
        g_nExitCode = IDCANCEL;

        // Create the main dialog (as a modeless dialog) from the localized template
        HWND hwnd = Lang_CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDialogProc, 0);
        if (!hwnd)
        {
            tstring_t strFail = LoadStr(IDS_FAILDIALOG);
            MessageBox(NULL, strFail.c_str(), NULL, MB_ICONERROR);
            return -2;
        }

        // Show it now
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        // The message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            // Process the modeless dialog message
            if (IsDialogMessage(hwnd, &msg))
                continue;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_nExitCode != psh1)
            break; // Exit now

        // Show settings dialog (as a modal dialog)
        Lang_DialogBox(hInstance, MAKEINTRESOURCE(IDD_SETTINGS), NULL, SettingsDialogProc, 0);
    }

    if (s_hSingleInstance)
    {
        CloseHandle(s_hSingleInstance);
        s_hSingleInstance = NULL;
    }

    return 0;
}
