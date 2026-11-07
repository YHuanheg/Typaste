// Typaste.cpp --- Typing + Paste
// Copyright (C) 2019-2026 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
//
// The application is split into small modules:
//   Lang.*      --- localized resources
//   AutoType.*  --- the typing engine
//   Settings.*  --- the settings and the dialogs
//   Config.*    --- Typaste.ini / registry storage
//   Clipboard.* --- system clipboard access
//   Dpi.*       --- Per-Monitor V2 helpers
//   Typaste.cpp --- this file: the main window, the tray icon and the hot-key
#include "Typaste.hpp"
#include "App.hpp"
#include "Settings.hpp"
#include "Config.hpp"
#include "Clipboard.hpp"
#include "Dpi.hpp"
#include "Util.hpp"
#include <vector>

// The global variables
HINSTANCE    g_hInst         = NULL;
HWND         g_hwndMain      = NULL;
HICON        g_hIcon         = NULL;
HICON        g_hIconSm       = NULL;
TCHAR        g_szNone[64]    = TEXT("(None)");
volatile LONG g_bAbortTyping = 0;

APP_SETTINGS g_settings;

static volatile LONG s_bTyping = 0;            // Is the typing thread running?
static HANDLE s_hTypingThread = NULL;          // The typing thread
static HANDLE s_hSingleInstance = NULL;        // The single instance mutex

static NOTIFYICONDATA s_nid;                   // The tray icon
static BOOL s_bTrayAdded = FALSE;              // Is the tray icon added?
static BOOL s_bRebuildMain = FALSE;            // Re-create the main window?

// Register a hot-key
BOOL MyRegisterHotKey(HWND hwnd)
{
    const UINT vk = LOBYTE(g_settings.wHotKey), flags = HIBYTE(g_settings.wHotKey);

    UINT nMod = 0;
    if (flags & HOTKEYF_ALT)
        nMod |= MOD_ALT;
    if (flags & HOTKEYF_CONTROL)
        nMod |= MOD_CONTROL;
    if (flags & HOTKEYF_SHIFT)
        nMod |= MOD_SHIFT;

    UnregisterHotKey(hwnd, HOTKEY_ID);

    if (RegisterHotKey(hwnd, HOTKEY_ID, nMod, vk))
        return TRUE;

    tstring_t strFail = LoadStr(IDS_FAILHOTKEY);
    if (strFail.empty())
        strFail = TEXT("Failed to register the hot-key.");
    MessageBox(hwnd, strFail.c_str(), NULL, MB_ICONERROR);

    return FALSE;
}

// Build a readable hot-key name like "Ctrl + V"
static tstring_t FormatHotKey(WORD wHotKey)
{
    const UINT vk = LOBYTE(wHotKey), flags = HIBYTE(wHotKey);
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

static void UpdateHotKeyText(HWND hwnd)
{
    tstring_t strText = FormatStr(LoadStr(IDS_HOTKEY_FMT), FormatHotKey(g_settings.wHotKey).c_str());
    SetDlgItemText(hwnd, txtHotkey, strText.c_str());
}

static void UpdateStatusText(HWND hwnd, LPCTSTR pszOverride)
{
    tstring_t strText;
    if (pszOverride && *pszOverride)
        strText = pszOverride;
    else
        strText = LoadStr(s_bTyping ? IDS_STATUS_TYPING : IDS_STATUS_READY);
    if (strText.empty())
        strText = s_bTyping ? TEXT("Typing...") : TEXT("Ready");
    SetDlgItemText(hwnd, txtStatus, strText.c_str());
}

// Show the clipboard text in the edit box of the main window
static void UpdateClipboardPreview(HWND hwnd)
{
    HWND hEdit = GetDlgItem(hwnd, edtClip);
    if (!hEdit)
        return;

    // Do not disturb the user while they are editing
    if (GetFocus() == hEdit)
        return;

    tstring_t strText;
    if (!Clip_GetText(strText))
        strText.clear();

    const int cch = GetWindowTextLength(hEdit);
    if (cch == (int)strText.size())
    {
        std::vector<TCHAR> buf((size_t)cch + 1, 0);
        GetWindowText(hEdit, &buf[0], cch + 1);
        if (strText == &buf[0])
            return; // already up to date
    }

    SetWindowText(hEdit, strText.c_str());
}

// WM_CLIPBOARDUPDATE
static void OnClipboardUpdate(HWND hwnd)
{
    UpdateClipboardPreview(hwnd);
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

        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        PostMessage(hwnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
        break;
    }
    }
}

//////////////////////////////////////////////////////////////////////////////
// The main window

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

// IDD_ABOUT
static INT_PTR CALLBACK AboutDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
        Dpi_InitTracking(hwnd);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);
        SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIcon);
        Dpi_SetupTitleFont(hwnd);
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

    MySleep(pJob->dwDelayToStart, pJob->bRandom);
    WaitModifierRelease(pJob->dwDelayToType, pJob->bRandom);

    if (pJob->bControlIME && pJob->hwndIme && pJob->bImeOn)
    {
        SendMessage(pJob->hwndIme, WM_IME_CONTROL, IMC_SETOPENSTATUS, FALSE);
        Sleep(300);
    }

    AutoType(pJob->text.c_str(), pJob->dwDelayToType,
             pJob->sound.empty() ? NULL : pJob->sound.c_str(), pJob->bRandom);

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

// Show a short message in the status line and restore it later
#define TIMER_STATUS 1

static void ShowStatusMessage(HWND hwnd, LPCTSTR pszText)
{
    UpdateStatusText(hwnd, pszText);
    SetTimer(hwnd, TIMER_STATUS, 2500, NULL);
}

// Copy the text of the edit box (or the selection) to the clipboard
static void OnCopyToClipboard(HWND hwnd)
{
    HWND hEdit = GetDlgItem(hwnd, edtClip);
    tstring_t strText;
    if (!Clip_GetEditSelectionOrAll(hEdit, strText, NULL) || strText.empty())
        return;

    if (Clip_SetText(strText))
    {
        tstring_t strCopied = LoadStr(IDS_COPIED);
        if (strCopied.empty())
            strCopied = TEXT("Copied to the clipboard.");
        ShowStatusMessage(hwnd, strCopied.c_str());
    }
}

// IDD_MAIN WM_INITDIALOG
static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hwndFocus);
    UNREFERENCED_PARAMETER(lParam);

    g_hwndMain = hwnd;

    SetProp(hwnd, MAINWINDOW_PROP, (HANDLE)1);
    Dpi_InitTracking(hwnd);

    Settings_Load();

    const BOOL bHotKey = MyRegisterHotKey(hwnd);

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);
    SendDlgItemMessage(hwnd, ico1, STM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIcon);
    Dpi_SetupTitleFont(hwnd);

    AddTrayIcon(hwnd);

    // Connect to the system clipboard
    AddClipboardFormatListener(hwnd);
    UpdateClipboardPreview(hwnd);

    UpdateHotKeyText(hwnd);
    UpdateStatusText(hwnd, NULL);

    if (!bHotKey)
        PostMessage(hwnd, WM_COMMAND, psh1, 0);

    return TRUE;
}

// IDD_MAIN WM_COMMAND psh2
static void OnOpenReadMe(HWND hwnd)
{
    tstring_t strReadMe = LoadStr(IDS_README);
    if (strReadMe.empty())
        strReadMe = TEXT("\\README.txt");

    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, strReadMe.c_str());

    ShellExecute(hwnd, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
}

// IDD_MAIN WM_COMMAND
static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    // This hwnd is a modeless dialog. We use DestroyWindow instead of EndDialog for this hwnd.
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
    case IDM_EXIT:
        DestroyWindow(hwnd);
        break;

    case psh1:
    case IDM_SETTINGS:
        Settings_Show(hwnd);
        // The settings may have changed: rebuild the window so that the
        // language, the hot-key and the delays are applied at once.
        s_bRebuildMain = TRUE;
        DestroyWindow(hwnd);
        break;

    case psh2:
        OnOpenReadMe(hwnd);
        break;

    case pshCopy:
        OnCopyToClipboard(hwnd);
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
static void NormalCtrlV(HWND hwnd, BOOL bRandom)
{
    UnregisterHotKey(hwnd, HOTKEY_ID);
    WaitModifierRelease(g_settings.dwDelayToType, bRandom);
    CtrlV(g_settings.dwDelayToType, bRandom);
    MyRegisterHotKey(hwnd);
}

// IDD_MAIN WM_HOTKEY
static void OnHotKey(HWND hwnd, int idHotKey, UINT fuModifiers, UINT vk)
{
    UNREFERENCED_PARAMETER(fuModifiers);
    UNREFERENCED_PARAMETER(vk);

    if (idHotKey != HOTKEY_ID)
        return;

    if (s_bTyping)
    {
        tstring_t strBusy = LoadStr(IDS_BUSY);
        if (!strBusy.empty())
            MessageBox(hwnd, strBusy.c_str(), NULL, MB_ICONINFORMATION);
        return;
    }

    // Respect the program whitelist: outside of it, Ctrl+V works as usual
    if (!Settings_IsForegroundWhitelisted())
    {
        UnregisterHotKey(hwnd, HOTKEY_ID);
        CtrlV(g_settings.dwDelayToType, g_settings.bRandom);
        MyRegisterHotKey(hwnd);
        return;
    }

    // The text to type: the selection of the preview, then the clipboard,
    // then the whole preview text.
    tstring_t strText;
    BOOL bSelection = FALSE;
    HWND hEdit = GetDlgItem(hwnd, edtClip);
    if (hEdit)
        Clip_GetEditSelectionOrAll(hEdit, strText, &bSelection);

    // Without a selection, prefer the real clipboard contents
    if (!bSelection)
    {
        tstring_t strClip;
        if (Clip_GetText(strClip) && !strClip.empty())
            strText = strClip;
    }

    if (strText.empty())
    {
        NormalCtrlV(hwnd, g_settings.bRandom);
        return;
    }

    if (strText.size() > MAX_TYPING_CHARS)
    {
        tstring_t strTooLong = FormatInt(LoadStr(IDS_TEXT_TOO_LONG), MAX_TYPING_CHARS);
        MessageBox(hwnd, strTooLong.c_str(), NULL, MB_ICONWARNING);
        return;
    }

    // Prepare for the sound file
    TCHAR szSoundFile[MAX_PATH];
    LPCTSTR pszSound = g_settings.strSound.c_str();
    if (g_settings.strSound == g_szNone)
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
    HWND hwndIme = ImmGetDefaultIMEWnd(hwndTarget);
    const BOOL bImeOn = (BOOL)SendMessage(hwndIme, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);

    TYPE_JOB *pJob = new TYPE_JOB();
    pJob->text = strText;
    pJob->sound = szSoundFile;
    pJob->dwDelayToType = g_settings.dwDelayToType;
    pJob->dwDelayToStart = g_settings.dwDelayToStart;
    pJob->bRandom = g_settings.bRandom;
    pJob->bControlIME = g_settings.bControlIME;
    pJob->hwndIme = hwndIme;
    pJob->bImeOn = bImeOn;

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
        NormalCtrlV(hwnd, g_settings.bRandom);
        return;
    }

    UpdateStatusText(hwnd, NULL);
}

// IDD_MAIN WM_DESTROY
static void OnDestroy(HWND hwnd)
{
    Settings_Save();

    InterlockedExchange(&g_bAbortTyping, 1);
    if (s_hTypingThread)
    {
        WaitForSingleObject(s_hTypingThread, 3000);
        CloseHandle(s_hTypingThread);
        s_hTypingThread = NULL;
    }

    RemoveClipboardFormatListener(hwnd);
    RemoveTrayIcon();
    UnregisterHotKey(hwnd, HOTKEY_ID);
    RemoveProp(hwnd, MAINWINDOW_PROP);
    Dpi_Forget(hwnd);

    if (g_hwndMain == hwnd)
        g_hwndMain = NULL;

    PostQuitMessage(0);
}

#ifndef HANDLE_WM_HOTKEY
    #define HANDLE_WM_HOTKEY(hwnd,wParam,lParam,fn) \
        ((fn)((hwnd),(int)(wParam),(UINT)LOWORD(lParam),(UINT)HIWORD(lParam)),(LRESULT)0)
#endif

// The main dialog IDD_MAIN procedure
static INT_PTR CALLBACK
MainDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_APP_TYPING_DONE)
    {
        if (s_hTypingThread)
        {
            CloseHandle(s_hTypingThread);
            s_hTypingThread = NULL;
        }
        UpdateStatusText(hwnd, NULL);
        return 0;
    }

    if (uMsg == WM_APP_TRAY)
    {
        OnTrayMessage(hwnd, lParam);
        return 0;
    }

    if (uMsg == WM_CLIPBOARDUPDATE)
    {
        OnClipboardUpdate(hwnd);
        return 0;
    }

    if (uMsg == WM_TIMER && wParam == TIMER_STATUS)
    {
        KillTimer(hwnd, TIMER_STATUS);
        UpdateStatusText(hwnd, NULL);
        return 0;
    }

    if (uMsg == WM_DPICHANGED)
        return Dpi_HandleChanged(hwnd, wParam, lParam);

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
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_hInst = hInstance;

    std::srand((unsigned int)::GetTickCount());

    // Choose the language before any resource is loaded
    Lang_Init(hInstance);

    // Load the settings; migrate them into Typaste.ini on the first portable run
    Settings_Load();
    if (!Config_IsIniMode() && Config_EnsureFile())
        Settings_Save();

    // Only one instance per user session
    s_hSingleInstance = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_NAME);
    if (s_hSingleInstance && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (HWND hwndMain = FindMainWindow())
            ShowMainWindow(hwndMain);
        CloseHandle(s_hSingleInstance);
        return 0;
    }

    InitCommonControls();

    g_hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    g_hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

    // The loop
    for (;;)
    {
        s_bRebuildMain = FALSE;

        HWND hwnd = Lang_CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDialogProc, 0);
        if (!hwnd)
        {
            tstring_t strFail = LoadStr(IDS_FAILDIALOG);
            MessageBox(NULL, strFail.c_str(), NULL, MB_ICONERROR);
            return -2;
        }

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (IsDialogMessage(hwnd, &msg))
                continue;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // The settings changed: re-create the main window in the new language
        if (!s_bRebuildMain)
            break;
    }

    RemoveTrayIcon();

    if (s_hSingleInstance)
    {
        CloseHandle(s_hSingleInstance);
        s_hSingleInstance = NULL;
    }

    return 0;
}
