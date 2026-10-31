// Typaste.cpp --- Typing + Paste
// Copyright (C) 2019-2026 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
#include "Typaste.hpp"
#include <vector>
#include <algorithm>
#include <cwctype>

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
#define SETTINGS_VERSION 3

// The maximum number of characters to type at once
#define MAX_TYPING_CHARS 100000

// The notifications of this application
#define WM_APP_TYPING_DONE (WM_APP + 1)
#define WM_APP_TRAY        (WM_APP + 2)

// The tray icon id
#define TRAY_ICON_ID 1

// WM_DPICHANGED needs Windows 8.1 headers; define it for older SDKs / targets
#ifndef WM_DPICHANGED
    #define WM_DPICHANGED 0x02E0
#endif

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

static NOTIFYICONDATA s_nid;                       // The tray icon
static BOOL s_bTrayAdded = FALSE;                  // Is the tray icon added?

static BOOL s_bOnlyWhitelist = FALSE;              // Only work in the whitelisted programs?
static std::vector<tstring_t> s_whitelist;         // The program whitelist (lower case base names)
static std::vector<tstring_t> s_editWhitelist;     // The working copy in the whitelist dialog

static UINT s_nDpi = 96;                           // The DPI of the main window
static HFONT s_hFontTitle = NULL;                  // The bold title font

// Forward declarations
BOOL Settings_Save(HWND hwnd);

// The window property that holds the DPI of a dialog
#define DPI_PROP TEXT("Typaste.Dpi")

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

// Format a message that takes one integer argument ("%d")
static tstring_t FormatInt(const tstring_t& fmt, int nValue)
{
    TCHAR szText[512] = TEXT("");
    if (fmt.empty())
        return szText;
    wsprintf(szText, fmt.c_str(), nValue);
    return szText;
}

// Get the DPI of a window (Windows 10+; falls back to the device caps)
typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

static UINT GetWindowDpi(HWND hwnd)
{
    static PFN_GetDpiForWindow s_pfnGetDpiForWindow = (PFN_GetDpiForWindow)
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (s_pfnGetDpiForWindow)
        return s_pfnGetDpiForWindow(hwnd);

    UINT nDpi = 96;
    if (HDC hdc = GetDC(hwnd))
    {
        nDpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
    }
    return nDpi ? nDpi : 96;
}

// Re-create the bold title font for the current DPI
static void ResetTitleFont(void)
{
    if (s_hFontTitle)
    {
        DeleteObject(s_hFontTitle);
        s_hFontTitle = NULL;
    }
}

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

// Scale the child controls and their fonts when the DPI changes
static void ScaleChildrenForDpi(HWND hwnd, UINT nOldDpi, UINT nNewDpi)
{
    if (!nOldDpi || !nNewDpi || nOldDpi == nNewDpi)
        return;

    const double k = (double)nNewDpi / (double)nOldDpi;

    for (HWND hChild = GetWindow(hwnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT))
    {
        RECT rc;
        GetWindowRect(hChild, &rc);
        MapWindowPoints(NULL, hwnd, (LPPOINT)&rc, 2);

        const int x = (int)(rc.left * k + 0.5);
        const int y = (int)(rc.top * k + 0.5);
        const int cx = (int)((rc.right - rc.left) * k + 0.5);
        const int cy = (int)((rc.bottom - rc.top) * k + 0.5);
        SetWindowPos(hChild, NULL, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);

        HFONT hFont = (HFONT)SendMessage(hChild, WM_GETFONT, 0, 0);
        LOGFONT lf;
        ZeroMemory(&lf, sizeof(lf));
        if (hFont && GetObject(hFont, sizeof(lf), &lf) && lf.lfHeight)
        {
            lf.lfHeight = (LONG)(lf.lfHeight * k);
            lf.lfWidth = (LONG)(lf.lfWidth * k);
            if (HFONT hNewFont = CreateFontIndirect(&lf))
                SendMessage(hChild, WM_SETFONT, (WPARAM)hNewFont, TRUE);
        }
    }

    // The title font has to be rebuilt for the new DPI
    ResetTitleFont();
    SetupTitleFont(hwnd);
}

// Remember the DPI of a dialog so that WM_DPICHANGED can compute the scale
static void InitDpiTracking(HWND hwnd)
{
    SetProp(hwnd, DPI_PROP, (HANDLE)(UINT_PTR)GetWindowDpi(hwnd));
}

// Handle WM_DPICHANGED: resize the dialog, scale the children and the fonts
static BOOL HandleDpiChanged(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    const UINT nNewDpi = HIWORD(wParam);
    const RECT *prc = (const RECT *)lParam;
    const UINT nOldDpi = (UINT)(UINT_PTR)GetProp(hwnd, DPI_PROP);

    if (prc)
    {
        SetWindowPos(hwnd, NULL, prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    ScaleChildrenForDpi(hwnd, nOldDpi ? nOldDpi : nNewDpi, nNewDpi);
    SetProp(hwnd, DPI_PROP, (HANDLE)(UINT_PTR)nNewDpi);
    return TRUE;
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

//////////////////////////////////////////////////////////////////////////////
// The program whitelist

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
    StrTrim(szPath, TEXT(" \t\r\n"));

    const TCHAR *pszBase = PathFindFileName(szPath);
    tstring_t name = (pszBase && *pszBase) ? pszBase : szPath;
    for (size_t i = 0; i < name.size(); ++i)
        name[i] = (TCHAR)towlower(name[i]);
    return name;
}

static BOOL WhitelistContains(const tstring_t& name)
{
    for (size_t i = 0; i < s_whitelist.size(); ++i)
    {
        if (lstrcmpi(s_whitelist[i].c_str(), name.c_str()) == 0)
            return TRUE;
    }
    return FALSE;
}

// Should the current foreground window be handled by Typaste?
static BOOL IsForegroundWhitelisted(void)
{
    if (!s_bOnlyWhitelist)
        return TRUE;

    HWND hwndTarget = GetForegroundWindow();
    if (!hwndTarget)
        return FALSE;

    tstring_t name = GetWindowProcessName(hwndTarget);
    if (name.empty())
        return FALSE;

    return WhitelistContains(name);
}

// Save the whitelist as a REG_MULTI_SZ value
static void WhitelistSaveToRegistry(HKEY hKey)
{
    std::vector<TCHAR> buf;
    for (size_t i = 0; i < s_whitelist.size(); ++i)
    {
        const tstring_t& name = s_whitelist[i];
        buf.insert(buf.end(), name.begin(), name.end());
        buf.push_back(0);
    }
    buf.push_back(0); // double NUL terminator

    const DWORD cbData = (DWORD)(buf.size() * sizeof(TCHAR));
    RegSetValueEx(hKey, TEXT("Whitelist"), 0, REG_MULTI_SZ, (const BYTE *)&buf[0], cbData);
}

// Load the whitelist from a REG_MULTI_SZ value
static void WhitelistLoadFromRegistry(HKEY hKey)
{
    DWORD dwType = 0, cbData = 0;
    if (RegQueryValueEx(hKey, TEXT("Whitelist"), NULL, &dwType, NULL, &cbData) != ERROR_SUCCESS)
        return;
    if (dwType != REG_MULTI_SZ || cbData < sizeof(TCHAR))
        return;

    std::vector<TCHAR> buf(cbData / sizeof(TCHAR) + 2, 0);
    if (RegQueryValueEx(hKey, TEXT("Whitelist"), NULL, NULL, (BYTE *)&buf[0], &cbData) != ERROR_SUCCESS)
        return;

    s_whitelist.clear();
    for (size_t i = 0; i < buf.size(); )
    {
        const TCHAR *psz = &buf[i];
        if (!*psz)
            break;
        tstring_t name = NormalizeProgramName(psz);
        if (!name.empty())
            s_whitelist.push_back(name);
        i += lstrlen(psz) + 1;
    }
}

//////////////////////////////////////////////////////////////////////////////
// The tray icon

static void AddTrayIcon(HWND hwnd)
{
    ZeroMemory(&s_nid, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = hwnd;
    s_nid.uID = TRAY_ICON_ID;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_APP_TRAY;
    s_nid.hIcon = g_hIconSm ? g_hIconSm : g_hIcon;

    tstring_t strTip = LoadStr(IDS_TRAY_TIP);
    lstrcpyn(s_nid.szTip, strTip.c_str(), _countof(s_nid.szTip));

    s_bTrayAdded = Shell_NotifyIcon(NIM_ADD, &s_nid) != FALSE;
    if (s_bTrayAdded)
    {
        s_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &s_nid);
    }
}

static void RemoveTrayIcon(void)
{
    if (s_bTrayAdded)
    {
        Shell_NotifyIcon(NIM_DELETE, &s_nid);
        s_bTrayAdded = FALSE;
    }
}

static void ShowMainWindow(HWND hwnd)
{
    ShowWindow(hwnd, SW_SHOW);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

// Handle the tray icon messages
static void OnTrayMessage(HWND hwnd, LPARAM lParam)
{
    switch (LOWORD(lParam))
    {
    case WM_LBUTTONDBLCLK:
        ShowMainWindow(hwnd);
        break;

    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
    {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        if (!hMenu)
            break;

        AppendMenu(hMenu, MF_STRING, IDM_SHOW, LoadStr(IDS_TRAY_SHOW).c_str());
        AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, LoadStr(IDS_TRAY_SETTINGS).c_str());
        AppendMenu(hMenu, MF_STRING, IDM_ABOUT, LoadStr(IDS_TRAY_ABOUT).c_str());
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, IDM_EXIT, LoadStr(IDS_TRAY_EXIT).c_str());

        // Required so that the menu closes when the user clicks elsewhere
        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        PostMessage(hwnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
        break;
    }
    }
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

// The program whitelist dialog IDD_WHITELIST

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
    {
        tstring_t name = NormalizeProgramName(szPath);
        Whitelist_AddName(hwnd, name);
    }
}

static void Whitelist_OnRemove(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, lstPrograms);
    if (!hList)
        return;

    int nSel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
    if (nSel < 0 || (size_t)nSel >= s_editWhitelist.size())
        return;

    s_editWhitelist.erase(s_editWhitelist.begin() + nSel);
    Whitelist_FillList(hwnd);
}

BOOL Whitelist_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);

    s_editWhitelist = s_whitelist;
    CheckDlgButton(hwnd, chkWhitelist, s_bOnlyWhitelist ? BST_CHECKED : BST_UNCHECKED);
    Whitelist_FillList(hwnd);
    SetForegroundWindow(hwnd);
    return TRUE;
}

void Whitelist_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    switch (id)
    {
    case IDOK:
        s_whitelist = s_editWhitelist;
        s_bOnlyWhitelist = (IsDlgButtonChecked(hwnd, chkWhitelist) == BST_CHECKED);
        Settings_Save(hwnd);
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

INT_PTR CALLBACK
WhitelistDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
// The settings dialog

// WM_INITDIALOG
BOOL Settings_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    InitDpiTracking(hwnd);

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
    s_bOnlyWhitelist = FALSE;
    s_whitelist.clear();

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

    cbData = sizeof(DWORD);
    error = RegQueryValueEx(hKey, TEXT("OnlyWhitelist"), NULL, NULL, (LPBYTE)&dwData, &cbData);
    if (!error)
    {
        s_bOnlyWhitelist = !!dwData;
    }

    WhitelistLoadFromRegistry(hKey);

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
    RegSetValueEx(hAppKey, TEXT("OnlyWhitelist"), 0, REG_DWORD, (const BYTE *)&s_bOnlyWhitelist, sizeof(s_bOnlyWhitelist));

    DWORD dwData = g_wHotKey;
    RegSetValueEx(hAppKey, TEXT("HotKey"), 0, REG_DWORD, (const BYTE *)&dwData, sizeof(dwData));

    DWORD cbData = (DWORD)((g_strSound.size() + 1) * sizeof(TCHAR));
    RegSetValueEx(hAppKey, TEXT("Sound"), 0, REG_SZ, (const BYTE *)g_strSound.c_str(), cbData);

    WhitelistSaveToRegistry(hAppKey);

    // The language choice (a BCP-47 tag such as "zh-CN", or "auto")
    const wchar_t *pszLang = Lang_GetTag(Lang_GetSetting());
    RegSetValueExW(hAppKey, L"Language", 0, REG_SZ, (const BYTE *)pszLang,
                   (DWORD)((wcslen(pszLang) + 1) * sizeof(wchar_t)));

    // Close the registry key
    RegCloseKey(hAppKey);

    return TRUE;
}

// IDD_SETTINGS WM_COMMAND btnReset
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
    case btnWhitelist:
        Lang_DialogBox(g_hInst, MAKEINTRESOURCE(IDD_WHITELIST), hwnd, WhitelistDialogProc, 0);
        break;
    }
}

// The dialog procedure of settings dialog IDD_SETTINGS
INT_PTR CALLBACK
SettingsDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DPICHANGED)
        return HandleDpiChanged(hwnd, wParam, lParam);

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

    // Track the DPI of this window
    InitDpiTracking(hwnd);

    // Load settings
    Settings_Load(hwnd);

    // Register the hot-key; if that fails, open the settings right away
    BOOL bHotKey = MyRegisterHotKey(hwnd);

    // Set the icons to the dialog
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);
    SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIcon);
    SetupTitleFont(hwnd);

    // Add the tray icon
    AddTrayIcon(hwnd);

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
    case IDCANCEL:
    case IDM_EXIT:
        g_nExitCode = IDCANCEL;
        DestroyWindow(hwnd);
        break;

    case psh1:
    case IDM_SETTINGS:
        g_nExitCode = psh1;
        DestroyWindow(hwnd);
        break;

    case psh2:
        OnOpenReadMe(hwnd);
        break;

    case psh4:
    case IDM_ABOUT:
        Lang_DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDialogProc, 0);
        break;

    case IDM_SHOW:
        ShowMainWindow(hwnd);
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

    // Respect the program whitelist: outside of it, Ctrl+V works as usual
    if (!IsForegroundWhitelisted())
    {
        // Just pass the original Ctrl+V through: no interception at all
        UnregisterHotKey(hwnd, HOTKEY_ID);
        CtrlV(g_dwDelayToType, g_bRandom);
        MyRegisterHotKey(hwnd);
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
        tstring_t strTooLong = FormatInt(LoadStr(IDS_TEXT_TOO_LONG), MAX_TYPING_CHARS);
        MessageBox(hwnd, strTooLong.c_str(), NULL, MB_ICONWARNING);
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

    // Remove the tray icon, the hot-key and the window mark
    RemoveTrayIcon();
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

    if (uMsg == WM_APP_TRAY)
    {
        OnTrayMessage(hwnd, lParam);
        return 0;
    }

    if (uMsg == WM_DPICHANGED)
        return HandleDpiChanged(hwnd, wParam, lParam);

    // Minimize to the notification area instead of the task bar
    if (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MINIMIZE)
    {
        ShowWindow(hwnd, SW_HIDE);
        return TRUE;
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
            ShowMainWindow(hwndMain);
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

    RemoveTrayIcon();

    if (s_hSingleInstance)
    {
        CloseHandle(s_hSingleInstance);
        s_hSingleInstance = NULL;
    }

    return 0;
}
