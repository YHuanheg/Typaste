// AutoType.hpp --- the auto typing engine
void WaitModifierRelease(DWORD dwDelay, BOOL bRandom);
void AutoType(LPCTSTR psz, DWORD dwDelay, LPCTSTR pszSound, BOOL bRandom);
void CtrlV(DWORD dwDelay, BOOL bRandom);
void MySleep(DWORD delay, BOOL bRandom);

// Set to non-zero to stop the current auto typing as soon as possible
extern volatile LONG g_bAbortTyping;
