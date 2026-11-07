// Ime.hpp --- input method control around the typing
#pragma once

#include "Typaste.hpp"

// How the IME is handled while the text is typed
#define IME_CONTROL_OFF        0   // leave the IME alone
#define IME_CONTROL_CLOSE      1   // close the IME (the old behaviour)
#define IME_CONTROL_ALPHANUM   2   // keep the IME open but switch it to English

// The IMM32 messages used here (they are not in every SDK)
#ifndef IMC_GETOPENSTATUS
    #define IMC_GETOPENSTATUS     0x0005
#endif
#ifndef IMC_SETOPENSTATUS
    #define IMC_SETOPENSTATUS     0x0006
#endif
#ifndef IMC_GETCONVERSIONMODE
    #define IMC_GETCONVERSIONMODE 0x0001
#endif
#ifndef IMC_SETCONVERSIONMODE
    #define IMC_SETCONVERSIONMODE 0x0002
#endif

#ifndef IME_CMODE_ALPHANUMERIC
    #define IME_CMODE_ALPHANUMERIC 0x0000
#endif
#ifndef IME_CMODE_NATIVE
    #define IME_CMODE_NATIVE 0x0001
#endif

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

// Switch the conversion mode of an IME window (used by the adaptive typing)
void Ime_SetConversionMode(HWND hwndIme, DWORD dwMode);
