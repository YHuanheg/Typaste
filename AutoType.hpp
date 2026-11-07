// AutoType.hpp --- the auto typing engine
#pragma once

#include "Typaste.hpp"

// How the characters are produced
#define OUTPUT_AUTO         0   // ASCII by keyboard, the rest as Unicode (default)
#define OUTPUT_ADAPTIVE_IME 1   // switch the IME per character and use the keyboard
#define OUTPUT_UNICODE      2   // send everything as Unicode input (IME independent)

// How long to wait for the user to release the modifier keys (in milliseconds)
void WaitModifierRelease(DWORD dwDelay, BOOL bRandom);

// The typing options
struct AT_OPTIONS
{
    DWORD    dwDelay;      // delay between the characters
    LPCTSTR  pszSound;     // the sound file (may be NULL)
    BOOL     bRandom;      // randomize the interval?
    int      nStart;       // the UTF-16 index to start from
    int      nOutputMode;  // OUTPUT_*
    HWND     hwndIme;      // the IME window for OUTPUT_ADAPTIVE_IME (may be NULL)

    AT_OPTIONS()
        : dwDelay(20), pszSound(NULL), bRandom(FALSE), nStart(0),
          nOutputMode(OUTPUT_AUTO), hwndIme(NULL)
    {
    }
};

// Type the text from opt.nStart on. Returns the index of the first character
// that was not typed (equal to the text length when everything was typed).
int AutoType(LPCTSTR psz, const AT_OPTIONS& opt);

void CtrlV(DWORD dwDelay, BOOL bRandom);
void MySleep(DWORD delay, BOOL bRandom);

// Set to non-zero to stop the current auto typing as soon as possible
extern volatile LONG g_bAbortTyping;

// Set to non-zero to hold the typing at the next character boundary
extern volatile LONG g_bPauseTyping;

// The number of UTF-16 units typed so far (updated after every character)
extern volatile LONG g_nTypedChars;
