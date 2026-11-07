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
#include "Ime.hpp"
#include <vector>

// The global variables
HINSTANCE    g_hInst         = NULL;
HWND         g_hwndMain      = NULL;
HICON        g_hIcon         = NULL;
HICON        g_hIconSm       = NULL;
TCHAR        g_szNone[64]    = TEXT("(None)");
volatile LONG g_bAbortTyping = 0;
volatile LONG g_bPauseTyping = 0;

APP_SETTINGS g_settings;

static volatile LONG s_bTyping = 0;            // Is the typing thread running?
static HANDLE s_hTypingThread = NULL;          // The typing thread
static HANDLE s_hSingleInstance = NULL;        // The single instance mutex

static NOTIFYICONDATA s_nid;                   // The tray icon
static BOOL s_bTrayAdded = FALSE;              // Is the tray icon added?
static BOOL s_bRebuildMain = FALSE;            // Re-create the main window?
static BOOL s_bPaused = FALSE;                 // Is the typing paused?

// The job that is being typed, and the state needed to continue it later
static std::wstring s_jobText;
static std::wstring s_jobSound;
static std::wstring s_resumeText;
static std::wstring s_resumeSound;
static volatile LONG s_nResumePos = 0;

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
        if (s_bTyping)
        {
            tstring_t strPause = LoadStr(s_bPaused ? IDS_CONTINUE : IDS_PAUSE);
            AppendMenu(hMenu, MF_STRING, IDM_PAUSE, strPause.c_str());
        }
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
    DWORD dwImeMode;
    DWORD dwOutputMode;
    IME_STATE ime;
    int nStart;
};

// The typing thread: it keeps the UI responsive while the text is being typed
static DWORD WINAPI TypingThreadProc(LPVOID lpParam)
{
    TYPE_JOB *pJob = (TYPE_JOB *)lpParam;

    MySleep(pJob->dwDelayToStart, pJob->bRandom);
    WaitModifierRelease(pJob->dwDelayToType, pJob->bRandom);

    // Decide how the IME is handled for this job:
    //   adaptive -> the engine switches it for every character
    //   as is    -> the IME has to step aside completely, otherwise a TSF input
    //               method (UWP editors) still eats the characters
    int nImeMode = (int)pJob->dwImeMode;
    if (pJob->dwOutputMode == OUTPUT_ADAPTIVE_IME)
        nImeMode = IME_CONTROL_OFF;
    else if (pJob->dwOutputMode == OUTPUT_UNICODE)
        nImeMode = IME_CONTROL_CLOSE;

    if (nImeMode != IME_CONTROL_OFF && pJob->ime.bValid)
    {
        Ime_Apply(pJob->ime, nImeMode);
        Sleep(300);
    }

    // AutoType stops at a character boundary on pause/abort and returns the
    // index it stopped at, which is what "Continue" resumes from.
    AT_OPTIONS opt;
    opt.dwDelay = pJob->dwDelayToType;
    opt.pszSound = pJob->sound.empty() ? NULL : pJob->sound.c_str();
    opt.bRandom = pJob->bRandom;
    opt.nStart = pJob->nStart;
    opt.nOutputMode = (int)pJob->dwOutputMode;
    opt.hwndIme = pJob->ime.hwndIme;

    const int nDone = AutoType(pJob->text.c_str(), opt);

    Ime_Restore(pJob->ime);
    if (pJob->dwImeMode != IME_CONTROL_OFF && pJob->ime.bValid)
        Sleep(200);

    delete pJob;

    InterlockedExchange(&g_nTypedChars, nDone);
    InterlockedExchange(&g_bPauseTyping, 0);
    InterlockedExchange(&s_bTyping, 0);
    if (g_hwndMain)
        PostMessage(g_hwndMain, WM_APP_TYPING_DONE, 0, 0);

    return 0;
}

// The timers of the main window
#define TIMER_STATUS   1
#define TIMER_PROGRESS 2

// Show a short message in the status line and restore the real state later
static void ShowStatusMessage(HWND hwnd, LPCTSTR pszText)
{
    SetDlgItemText(hwnd, txtStatus, pszText);
    SetTimer(hwnd, TIMER_STATUS, 2500, NULL);
}

// Update the status line and the pause/continue buttons
static void UpdateTypingUI(HWND hwnd)
{
    const int nDone = (int)g_nTypedChars;
    const int nTotal = (int)s_jobText.size();

    tstring_t strStatus;
    if (s_bTyping)
    {
        if (s_bPaused)
            strStatus = FormatInt2(LoadStr(IDS_STATUS_PAUSED_FMT), nDone, nTotal);
        else
            strStatus = FormatInt2(LoadStr(IDS_STATUS_TYPING_FMT), nDone, nTotal);
    }
    else if (!s_resumeText.empty())
    {
        strStatus = FormatInt2(LoadStr(IDS_STATUS_STOPPED_FMT),
                               (int)s_nResumePos, (int)s_resumeText.size());
    }
    else
    {
        strStatus = LoadStr(IDS_STATUS_READY);
    }

    if (strStatus.empty())
        strStatus = s_bTyping ? TEXT("Typing...") : TEXT("Ready");
    SetDlgItemText(hwnd, txtStatus, strStatus.c_str());

    // Pause / Continue is only usable while something is being typed
    HWND hPause = GetDlgItem(hwnd, pshPause);
    if (hPause)
    {
        EnableWindow(hPause, s_bTyping ? TRUE : FALSE);
        tstring_t strPause = LoadStr(s_bPaused ? IDS_CONTINUE : IDS_PAUSE);
        SetWindowText(hPause, strPause.c_str());
    }

    // Continue is only usable when the last job was interrupted
    HWND hResume = GetDlgItem(hwnd, pshResume);
    if (hResume)
    {
        const BOOL bCanResume =
            (!s_bTyping && !s_resumeText.empty() &&
             (int)s_nResumePos < (int)s_resumeText.size());
        EnableWindow(hResume, bCanResume);
    }
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

// Start typing strText from nStart
static BOOL StartTyping(HWND hwnd, const tstring_t& strText, const tstring_t& strSound, int nStart)
{
    if (strText.empty())
        return FALSE;

    if (strText.size() > MAX_TYPING_CHARS)
    {
        tstring_t strTooLong = FormatInt(LoadStr(IDS_TEXT_TOO_LONG), MAX_TYPING_CHARS);
        MessageBox(hwnd, strTooLong.c_str(), NULL, MB_ICONWARNING);
        return FALSE;
    }

    if (nStart < 0)
        nStart = 0;
    if (nStart > (int)strText.size())
        nStart = (int)strText.size();

    // Remember the job so that "Continue" can resume it
    s_jobText = strText;
    s_jobSound = strSound;

    // This is the target window; remember its IME state so that it can be
    // restored when the typing is finished
    HWND hwndTarget = GetForegroundWindow();
    IME_STATE imeState;
    Ime_GetState(hwndTarget, imeState);

    TYPE_JOB *pJob = new TYPE_JOB();
    pJob->text = strText;
    pJob->sound = strSound;
    pJob->dwDelayToType = g_settings.dwDelayToType;
    pJob->dwDelayToStart = g_settings.dwDelayToStart;
    pJob->bRandom = g_settings.bRandom;
    pJob->dwImeMode = g_settings.dwImeMode;
    pJob->dwOutputMode = g_settings.dwOutputMode;
    pJob->ime = imeState;
    pJob->nStart = nStart;

    InterlockedExchange(&g_bAbortTyping, 0);
    InterlockedExchange(&g_bPauseTyping, 0);
    InterlockedExchange(&g_nTypedChars, nStart);
    InterlockedExchange(&s_bTyping, 1);
    s_bPaused = FALSE;

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
        return FALSE;
    }

    SetTimer(hwnd, TIMER_PROGRESS, 200, NULL);
    UpdateTypingUI(hwnd);
    return TRUE;
}

// Pause / Continue
static void OnPauseToggle(HWND hwnd)
{
    if (!s_bTyping)
        return;

    const LONG bNew = g_bPauseTyping ? 0 : 1;
    InterlockedExchange(&g_bPauseTyping, bNew);
    s_bPaused = bNew ? TRUE : FALSE;
    UpdateTypingUI(hwnd);
}

// Continue the interrupted job from where it stopped
static void OnResume(HWND hwnd)
{
    if (s_bTyping || s_resumeText.empty())
        return;

    const int nStart = (int)s_nResumePos;
    if (nStart >= (int)s_resumeText.size())
        return;

    tstring_t strText = s_resumeText;
    tstring_t strSound = s_resumeSound;

    s_resumeText.clear();
    s_resumeSound.clear();
    s_nResumePos = 0;

    if (!StartTyping(hwnd, strText, strSound, nStart))
        UpdateTypingUI(hwnd);
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
    UpdateTypingUI(hwnd);

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

    case pshPause:
    case IDM_PAUSE:
        OnPauseToggle(hwnd);
        break;

    case pshResume:
        OnResume(hwnd);
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

    // A new hot-key press starts a new job: the old resume point is dropped
    s_resumeText.clear();
    s_resumeSound.clear();
    s_nResumePos = 0;

    StartTyping(hwnd, strText, szSoundFile, 0);
}

// IDD_MAIN WM_DESTROY
static void OnDestroy(HWND hwnd)
{
    Settings_Save();

    InterlockedExchange(&g_bAbortTyping, 1);
    InterlockedExchange(&g_bPauseTyping, 0);
    if (s_hTypingThread)
    {
        WaitForSingleObject(s_hTypingThread, 3000);
        CloseHandle(s_hTypingThread);
        s_hTypingThread = NULL;
    }

    KillTimer(hwnd, TIMER_STATUS);
    KillTimer(hwnd, TIMER_PROGRESS);

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
        KillTimer(hwnd, TIMER_PROGRESS);
        InterlockedExchange(&g_bPauseTyping, 0);
        s_bPaused = FALSE;

        // Keep the interrupted job so that "Continue" can resume it
        const int nDone = (int)g_nTypedChars;
        if (nDone > 0 && nDone < (int)s_jobText.size())
        {
            s_resumeText = s_jobText;
            s_resumeSound = s_jobSound;
            s_nResumePos = nDone;
        }
        else
        {
            s_resumeText.clear();
            s_resumeSound.clear();
            s_nResumePos = 0;
        }

        UpdateTypingUI(hwnd);
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
        UpdateTypingUI(hwnd);
        return 0;
    }

    if (uMsg == WM_TIMER && wParam == TIMER_PROGRESS)
    {
        UpdateTypingUI(hwnd);
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
