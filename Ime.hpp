// Ime.hpp --- input method control around the typing
#pragma once

#include "Typaste.hpp"

// How the IME is handled while the text is typed
#define IME_CONTROL_OFF        0   // leave the IME alone
#define IME_CONTROL_CLOSE      1   // close the IME (the old behaviour)
#define IME_CONTROL_ALPHANUM   2   // keep the IME open but switch it to English

// The state of the IME of the target window, saved before the typing starts
struct IME_STATE
{
    BOOL  bValid;          // is hwndIme usable?
    HWND  hwndIme;         // the default IME window of the target
    BOOL  bOpen;           // is the IME open (Chinese mode)?
    DWORD dwConversion;    // IME_CMODE_* of the IME
};

// Read the current IME state of the target window
BOOL Ime_GetState(HWND hwndTarget, IME_STATE& state);

// Apply the configured policy. Returns TRUE when the state was changed.
BOOL Ime_Apply(const IME_STATE& state, int nMode);

// Put the IME back into the saved state
void Ime_Restore(const IME_STATE& state);
