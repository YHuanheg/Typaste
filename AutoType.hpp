#pragma once

// AutoType.hpp --- the auto typing engine
void WaitModifierRelease(DWORD dwDelay, BOOL bRandom);

// Type the text from nStart on. Returns the index of the first character that
// was not typed (equal to the text length when everything was typed).
int AutoType(LPCTSTR psz, DWORD dwDelay, LPCTSTR pszSound, BOOL bRandom, int nStart = 0);

void CtrlV(DWORD dwDelay, BOOL bRandom);
void MySleep(DWORD delay, BOOL bRandom);

// Set to non-zero to stop the current auto typing as soon as possible
extern volatile LONG g_bAbortTyping;

// Set to non-zero to hold the typing at the next character boundary
extern volatile LONG g_bPauseTyping;

// The number of UTF-16 units typed so far (updated after every character)
extern volatile LONG g_nTypedChars;
