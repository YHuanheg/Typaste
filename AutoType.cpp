// AutoType.cpp
// Copyright (C) 2019-2026 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
#include "Typaste.hpp"
#include <strsafe.h>
#include "Ime.hpp"

// How long to wait for the user to release the modifier keys (in milliseconds)
#define MODIFIER_RELEASE_TIMEOUT 5000

// With a zero delay the target may merge the input events. Sleep once per this
// many characters so that nothing is lost.
#define ZERO_DELAY_BATCH 16

// Emulate a key press or release
static inline void MyKeybdEvent(WORD wVk, WORD wScan, DWORD dwFlags, ULONG_PTR dwExtra)
{
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = wVk;
    input.ki.wScan = wScan;
    input.ki.dwFlags = dwFlags;
    input.ki.time = 0;
    input.ki.dwExtraInfo = dwExtra;
    SendInput(1, &input, sizeof(input));
}

void MySleep(DWORD delay, BOOL bRandom)
{
    if (!delay)
        return;

    if (!bRandom)
    {
        ::Sleep(delay);
        return;
    }

    DWORD new_delay = (delay * 2 / 5) + (std::rand() % delay) * 3 / 5;
    ::Sleep(new_delay);
}

// Wait for release of modifier keys
void WaitModifierRelease(DWORD dwDelay, BOOL bRandom)
{
    DWORD dwStart = GetTickCount();

    while (GetAsyncKeyState(VK_SHIFT) < 0 ||
           GetAsyncKeyState(VK_CONTROL) < 0 ||
           GetAsyncKeyState(VK_MENU) < 0)
    {
        // Do not hang forever when a key is stuck or held down on purpose
        if (GetTickCount() - dwStart >= MODIFIER_RELEASE_TIMEOUT)
            break;

        MySleep(dwDelay ? dwDelay : 10, bRandom);
    }
}

// Send UTF-16 code units as Unicode input, without any keyboard layout involved.
// This is the only reliable way to type characters such as CJK, emoji or symbols
// that VkKeyScanEx cannot map.
static void SendUnicodeText(LPCWSTR psz, int cch, DWORD dwDelay, BOOL bRandom)
{
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));

    int n = 0;
    for (int i = 0; i < cch && n < 2; ++i)
    {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wScan = psz[i];
        inputs[n].ki.dwFlags = KEYEVENTF_UNICODE;
        ++n;
    }

    const int cDown = n;
    for (int i = 0; i < cDown; ++i)
    {
        inputs[n] = inputs[i];
        inputs[n].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        ++n;
    }

    SendInput((UINT)n, inputs, sizeof(INPUT));
    MySleep(dwDelay, bRandom);
}

// Emulate key press and release
void EmulateKey(BYTE vk, BYTE flags, DWORD dwDelay, BOOL bRandom)
{
    // Emulate the modifier keys press
    if (flags & HOTKEYF_SHIFT)
    {
        MyKeybdEvent(VK_LSHIFT, 0, 0, 0);
        MySleep(dwDelay, bRandom);
    }
    if (flags & HOTKEYF_CONTROL)
    {
        MyKeybdEvent(VK_LCONTROL, 0, 0, 0);
        MySleep(dwDelay, bRandom);
    }
    if (flags & HOTKEYF_ALT)
    {
        MyKeybdEvent(VK_LMENU, 0, 0, 0);
        MySleep(dwDelay, bRandom);
    }

    // Emulate main key press
    MyKeybdEvent(vk, 0, 0, 0);
    MySleep(dwDelay, bRandom);

    // Emulate main key release
    MyKeybdEvent(vk, 0, KEYEVENTF_KEYUP, 0);
    MySleep(dwDelay, bRandom);

    // Emulate the modifier keys release (in reverse order)
    if (flags & HOTKEYF_ALT)
    {
        MyKeybdEvent(VK_LMENU, 0, KEYEVENTF_KEYUP, 0);
        MySleep(dwDelay, bRandom);
    }
    if (flags & HOTKEYF_CONTROL)
    {
        MyKeybdEvent(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
        MySleep(dwDelay, bRandom);
    }
    if (flags & HOTKEYF_SHIFT)
    {
        MyKeybdEvent(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
        MySleep(dwDelay, bRandom);
    }
}

// Emulate Ctrl+V or something
void CtrlV(DWORD dwDelay, BOOL bRandom)
{
    EmulateKey('V', HOTKEYF_CONTROL, dwDelay, bRandom);
}

// Switch the keyboard layout by pressing [Shift]+[Alt]
void SwitchKL(DWORD dwDelay, BOOL bRandom)
{
    MyKeybdEvent(VK_LSHIFT, 0, 0, 0);
    MySleep(dwDelay, bRandom);
    MyKeybdEvent(VK_LMENU, 0, 0, 0);
    MySleep(dwDelay, bRandom);
    MyKeybdEvent(VK_LMENU, 0, KEYEVENTF_KEYUP, 0);
    MySleep(dwDelay, bRandom);
    MyKeybdEvent(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
    MySleep(dwDelay, bRandom);
}

// The number of UTF-16 units typed so far
volatile LONG g_nTypedChars = 0;

// Switch the IME conversion mode just before a character is sent. ASCII goes
// through the keyboard, everything else needs the native (IME) mode.
static void SwitchImeForChar(HWND hwndIme, DWORD *pdwMode, TCHAR ch)
{
    if (!hwndIme)
        return;

    const DWORD dwWant = (ch < 0x80) ? IME_CMODE_ALPHANUMERIC : IME_CMODE_NATIVE;
    if (*pdwMode == dwWant)
        return;

    Ime_SetConversionMode(hwndIme, dwWant);
    *pdwMode = dwWant;
}

// The auto typing function
int AutoType(LPCTSTR psz, const AT_OPTIONS& opt)
{
    const UINT cMaxKL = 10; // Who uses 10 keyboards in a moment?

    const DWORD dwDelay = opt.dwDelay;
    const BOOL bRandom = opt.bRandom;
    LPCTSTR pszSound = opt.pszSound;

    // Get the list of the keyboard layouts
    HKL ahKLs[cMaxKL];
    UINT chkl = GetKeyboardLayoutList(cMaxKL, ahKLs);

    // Remember the old keyboard layout
    HKL hOldKL = GetKeyboardLayout(0);

    const int cchTotal = lstrlen(psz);
    int nPos = opt.nStart;
    if (nPos < 0)
        nPos = 0;
    if (nPos > cchTotal)
        nPos = cchTotal;

    UINT nSent = 0;
    DWORD dwImeMode = 0xFFFFFFFF; // unknown: force the first switch

    g_nTypedChars = nPos;

    // The typing loop
    while (nPos < cchTotal)
    {
        // Stop when the user asked us to
        if (g_bAbortTyping)
            break;

        // Hold here while the user has paused the typing
        while (g_bPauseTyping)
        {
            if (g_bAbortTyping)
                break;
            ::Sleep(50);
        }

        if (g_bAbortTyping)
            break;

        // Quit the loop if the user press Esc key
        if (GetAsyncKeyState(VK_ESCAPE) < 0)
            break;

        const TCHAR ch = psz[nPos];

        // Handle special characters: these are keys, not characters, in every
        // output mode
        switch (ch)
        {
        case L'\r': // CR
            ++nPos;
            g_nTypedChars = nPos;
            continue;
        case L'\n': // LF
            MyKeybdEvent(VK_RETURN, 0, 0, 0);
            MySleep(dwDelay, bRandom);
            MyKeybdEvent(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            MySleep(dwDelay, bRandom);
            ++nPos;
            g_nTypedChars = nPos;
            continue;
        case L'\t': // TAB
            MyKeybdEvent(VK_TAB, 0, 0, 0);
            MySleep(dwDelay, bRandom);
            MyKeybdEvent(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
            MySleep(dwDelay, bRandom);
            ++nPos;
            g_nTypedChars = nPos;
            continue;
        }

        // Play the sound if necessary
        if (pszSound && *pszSound)
        {
            PlaySound(pszSound, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
            MySleep(dwDelay, bRandom);
        }

        // How many UTF-16 units this character takes (surrogate pairs)
        int cchChar = 1;
        if (IS_HIGH_SURROGATE(WORD(ch)) && psz[nPos + 1] &&
            IS_LOW_SURROGATE(WORD(psz[nPos + 1])))
        {
            cchChar = 2;
        }

        // The "as is" mode: every character goes through SendInput as Unicode
        // input, so the keyboard layout and the IME state cannot interfere.
        if (opt.nOutputMode == OUTPUT_UNICODE)
        {
            SendUnicodeText(psz + nPos, cchChar, dwDelay, bRandom);
            nPos += cchChar;
            g_nTypedChars = nPos;

            if (dwDelay == 0 && (++nSent % ZERO_DELAY_BATCH) == 0)
                ::Sleep(1);
            continue;
        }

        // The adaptive mode: put the IME into the mode this character needs
        if (opt.nOutputMode == OUTPUT_ADAPTIVE_IME)
            SwitchImeForChar(opt.hwndIme, &dwImeMode, ch);

        // Translate the character to a virtual key
        SHORT s = -1;
        UINT iKL = 0;
        for (iKL = 0; iKL < chkl; ++iKL)
        {
            s = VkKeyScanEx(ch, ahKLs[iKL]);
            if (s != -1)
                break; // Success!
        }

        if (s == -1)
        {
            // The keyboard layouts cannot produce this character: send it as
            // Unicode input instead of WM_CHAR (CJK, emoji, any symbol).
            SendUnicodeText(psz + nPos, cchChar, dwDelay, bRandom);
            nPos += cchChar;
        }
        else
        {
            // Choose the prefer keyboard layout
            HKL hKL;
            for (INT k = 0; k < (INT)cMaxKL; ++k)
            {
                hKL = GetKeyboardLayout(0);
                if (hKL == ahKLs[iKL])
                    break; // The keyboard layout does match

                // Switch the keyboard layout by pressing [Shift]+[Alt]
                SwitchKL(dwDelay, bRandom);
            }

            BYTE vk = LOBYTE(s); // The translated virtual key code
            BYTE flags = HIBYTE(s); // The modifier flags

            // Emulate the key press and release
            EmulateKey(vk, flags, dwDelay, bRandom);
            ++nPos;
        }

        g_nTypedChars = nPos;

        // A zero delay may drop characters; give the target some breathing room
        if (dwDelay == 0 && (++nSent % ZERO_DELAY_BATCH) == 0)
            ::Sleep(1);
    }

    // Restore the keyboard layout
    for (INT k = 0; k < (INT)cMaxKL; ++k)
    {
        HKL hKL = GetKeyboardLayout(0);
        if (hKL == hOldKL)
            break; // The keyboard layout does match

        // Switch the keyboard layout by pressing [Shift]+[Alt]
        SwitchKL(dwDelay, bRandom);
    }

    return nPos;
}
