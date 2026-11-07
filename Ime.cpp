// Ime.cpp --- input method control around the typing
//
// Closing the IME (the behaviour up to 1.5.0) works, but it makes some IMEs
// tear down their composition window and lose the candidate state, and a few
// TSF-only IMEs reopen in Chinese mode anyway. Switching the conversion mode
// to IME_CMODE_ALPHANUMERIC keeps the IME alive and simply makes it pass the
// characters through, which is what the user actually wants.
#include "Typaste.hpp"
#include "Ime.hpp"

BOOL Ime_GetState(HWND hwndTarget, IME_STATE& state)
{
    ZeroMemory(&state, sizeof(state));

    if (!hwndTarget)
        return FALSE;

    state.hwndIme = ImmGetDefaultIMEWnd(hwndTarget);
    if (!state.hwndIme)
        return FALSE;

    state.bOpen = (BOOL)SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);
    state.dwConversion = (DWORD)SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_GETCONVERSIONMODE, 0);
    state.bValid = TRUE;
    return TRUE;
}

BOOL Ime_Apply(const IME_STATE& state, int nMode)
{
    if (!state.bValid || !IsWindow(state.hwndIme))
        return FALSE;
    switch (nMode)
    {
    case IME_CONTROL_CLOSE:
        if (state.bOpen)
            SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_SETOPENSTATUS, FALSE);
        return TRUE;

    case IME_CONTROL_ALPHANUM:
        // A closed IME already passes the characters through, so there is
        // nothing to do in that case.
        if (!state.bOpen)
            return FALSE;

        // Already in the English mode: do not touch it
        if ((state.dwConversion & IME_CMODE_NATIVE) == 0)
            return FALSE;

        SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_SETCONVERSIONMODE,
                    (WPARAM)IME_CMODE_ALPHANUMERIC);
        return TRUE;

    default:
        return FALSE;
    }
}

void Ime_Restore(const IME_STATE& state)
{
    if (!state.bValid || !IsWindow(state.hwndIme))
        return;

    // Restore the conversion mode first (Chinese punctuation and the shape
    // follow it), then the open state.
    SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_SETCONVERSIONMODE,
                (WPARAM)state.dwConversion);
    SendMessage(state.hwndIme, WM_IME_CONTROL, IMC_SETOPENSTATUS, state.bOpen ? TRUE : FALSE);
}

void Ime_SetConversionMode(HWND hwndIme, DWORD dwMode)
{
    if (!hwndIme || !IsWindow(hwndIme))
        return;
    SendMessage(hwndIme, WM_IME_CONTROL, IMC_SETCONVERSIONMODE, (WPARAM)dwMode);
}
