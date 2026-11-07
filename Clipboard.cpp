// Clipboard.cpp --- system clipboard helpers
#include "Typaste.hpp"
#include "Clipboard.hpp"
#include <vector>

#ifdef UNICODE
    #define CF_GENERICTEXT CF_UNICODETEXT
#else
    #define CF_GENERICTEXT CF_TEXT
#endif

BOOL Clip_GetText(tstring_t& str)
{
    str.clear();

    if (!IsClipboardFormatAvailable(CF_GENERICTEXT))
        return FALSE;

    if (!OpenClipboard(NULL))
        return FALSE;

    BOOL bResult = FALSE;
    if (HGLOBAL hText = GetClipboardData(CF_GENERICTEXT))
    {
        if (LPTSTR pszText = (LPTSTR)GlobalLock(hText))
        {
            str = pszText;
            GlobalUnlock(hText);
            bResult = TRUE;
        }
    }

    CloseClipboard();
    return bResult;
}

BOOL Clip_SetText(const tstring_t& str)
{
    if (!OpenClipboard(NULL))
        return FALSE;

    BOOL bResult = FALSE;
    if (EmptyClipboard())
    {
        const size_t cbData = (str.size() + 1) * sizeof(TCHAR);
        if (HGLOBAL hText = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, cbData))
        {
            if (void *pv = GlobalLock(hText))
            {
                memcpy(pv, str.c_str(), cbData);
                GlobalUnlock(hText);
                if (SetClipboardData(CF_GENERICTEXT, hText))
                    bResult = TRUE;
                else
                    GlobalFree(hText);
            }
            else
            {
                GlobalFree(hText);
            }
        }
    }

    CloseClipboard();
    return bResult;
}

BOOL Clip_GetEditSelectionOrAll(HWND hEdit, tstring_t& str, BOOL *pbSelection)
{
    str.clear();
    if (pbSelection)
        *pbSelection = FALSE;
    if (!hEdit)
        return FALSE;

    DWORD dwStart = 0, dwEnd = 0;
    SendMessage(hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    const int cchAll = GetWindowTextLength(hEdit);
    if (cchAll <= 0)
        return FALSE;

    std::vector<TCHAR> buf((size_t)cchAll + 1, 0);
    GetWindowText(hEdit, &buf[0], cchAll + 1);

    if (dwEnd > dwStart && dwEnd <= (DWORD)cchAll)
    {
        str.assign(&buf[0] + dwStart, dwEnd - dwStart);
        if (pbSelection)
            *pbSelection = TRUE;
        return TRUE;
    }

    str = &buf[0];
    return TRUE;
}
