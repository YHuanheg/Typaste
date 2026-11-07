// App.hpp --- shared declarations of the Typaste application
#pragma once

#include "Typaste.hpp"
#include <vector>

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

// The version of the settings layout
#define SETTINGS_VERSION 4

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

// The application settings
struct APP_SETTINGS
{
    DWORD dwDelayToType;                 // the delay to type (in milliseconds)
    DWORD dwDelayToStart;                // the delay to start (in milliseconds)
    WORD  wHotKey;                       // MAKEWORD(virtual key, HOTKEYF_*)
    BOOL  bControlIME;                   // control the IME on/off?
    BOOL  bRandom;                       // randomize the interval?
    BOOL  bOnlyWhitelist;                // only work in the whitelisted programs?
    tstring_t strSound;                  // the sound file name
    std::vector<tstring_t> whitelist;    // the program whitelist (lower case base names)
};

extern HINSTANCE    g_hInst;             // the instance handle
extern HWND         g_hwndMain;          // the main window
extern HICON        g_hIcon;             // the main icon
extern HICON        g_hIconSm;           // the small icon
extern TCHAR        g_szNone[64];        // "(None)"
extern volatile LONG g_bAbortTyping;     // stop the typing thread now?
extern APP_SETTINGS g_settings;          // the current settings
