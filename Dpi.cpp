// Dpi.cpp --- Per-Monitor V2 helpers
//
// The child controls are not scaled by their previous size (that accumulates
// rounding errors every time the window crosses a monitor boundary). Instead the
// layout is recorded once, normalized to 96 DPI, and re-applied for every DPI.
#include "Typaste.hpp"
#include "App.hpp"
#include "Dpi.hpp"
#include <map>
#include <vector>

#define DPI_PROP TEXT("Typaste.Dpi")

struct CTRL_BASE
{
    HWND hwnd;
    RECT rc;        // normalized to 96 DPI
    LOGFONT lf;     // normalized to 96 DPI
    BOOL bHasFont;
};

static std::map<HWND, std::vector<CTRL_BASE> > s_layouts;
static HFONT s_hFontTitle = NULL;

typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

UINT Dpi_GetWindowDpi(HWND hwnd)
{
    static PFN_GetDpiForWindow s_pfnGetDpiForWindow = (PFN_GetDpiForWindow)
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (s_pfnGetDpiForWindow)
        return s_pfnGetDpiForWindow(hwnd);

    UINT nDpi = 96;
    if (HDC hdc = GetDC(hwnd))
    {
        nDpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
    }
    return nDpi ? nDpi : 96;
}

void Dpi_Forget(HWND hwnd)
{
    s_layouts.erase(hwnd);
    RemoveProp(hwnd, DPI_PROP);
}

void Dpi_InitTracking(HWND hwnd)
{
    const UINT nDpi = Dpi_GetWindowDpi(hwnd);

    std::vector<CTRL_BASE> layout;
    for (HWND hChild = GetWindow(hwnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT))
    {
        CTRL_BASE base;
        ZeroMemory(&base, sizeof(base));
        base.hwnd = hChild;

        RECT rc;
        GetWindowRect(hChild, &rc);
        MapWindowPoints(NULL, hwnd, (LPPOINT)&rc, 2);

        // Normalize to 96 DPI
        base.rc.left = MulDiv(rc.left, 96, nDpi);
        base.rc.top = MulDiv(rc.top, 96, nDpi);
        base.rc.right = MulDiv(rc.right, 96, nDpi);
        base.rc.bottom = MulDiv(rc.bottom, 96, nDpi);

        HFONT hFont = (HFONT)SendMessage(hChild, WM_GETFONT, 0, 0);
        if (hFont && GetObject(hFont, sizeof(base.lf), &base.lf) && base.lf.lfHeight)
        {
            base.lf.lfHeight = MulDiv(base.lf.lfHeight, 96, nDpi);
            base.lf.lfWidth = MulDiv(base.lf.lfWidth, 96, nDpi);
            base.bHasFont = TRUE;
        }

        layout.push_back(base);
    }

    s_layouts[hwnd] = layout;
    SetProp(hwnd, DPI_PROP, (HANDLE)(UINT_PTR)nDpi);
}

void Dpi_SetupTitleFont(HWND hwnd)
{
    if (!s_hFontTitle)
    {
        HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        LOGFONT lf;
        ZeroMemory(&lf, sizeof(lf));
        if (hFont && GetObject(hFont, sizeof(lf), &lf))
        {
            lf.lfHeight = lf.lfHeight * 14 / 10;
            lf.lfWeight = FW_BOLD;
            s_hFontTitle = CreateFontIndirect(&lf);
        }
    }
    if (s_hFontTitle)
        SendDlgItemMessage(hwnd, txtTitle, WM_SETFONT, (WPARAM)s_hFontTitle, TRUE);
}

// Re-apply the normalized layout for the given DPI
static void ApplyLayout(HWND hwnd, UINT nDpi)
{
    std::map<HWND, std::vector<CTRL_BASE> >::iterator it = s_layouts.find(hwnd);
    if (it == s_layouts.end())
        return;

    for (size_t i = 0; i < it->second.size(); ++i)
    {
        const CTRL_BASE& base = it->second[i];
        if (!IsWindow(base.hwnd))
            continue;

        const int x = MulDiv(base.rc.left, nDpi, 96);
        const int y = MulDiv(base.rc.top, nDpi, 96);
        const int cx = MulDiv(base.rc.right - base.rc.left, nDpi, 96);
        const int cy = MulDiv(base.rc.bottom - base.rc.top, nDpi, 96);
        SetWindowPos(base.hwnd, NULL, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);

        if (base.bHasFont)
        {
            LOGFONT lf = base.lf;
            lf.lfHeight = MulDiv(base.lf.lfHeight, nDpi, 96);
            lf.lfWidth = MulDiv(base.lf.lfWidth, nDpi, 96);
            if (HFONT hFont = CreateFontIndirect(&lf))
                SendMessage(base.hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }

    // Rebuild the bold title font for the new DPI
    if (s_hFontTitle)
    {
        DeleteObject(s_hFontTitle);
        s_hFontTitle = NULL;
    }
    Dpi_SetupTitleFont(hwnd);
}

BOOL Dpi_HandleChanged(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    const UINT nNewDpi = HIWORD(wParam);
    const RECT *prc = (const RECT *)lParam;

    if (prc)
    {
        SetWindowPos(hwnd, NULL, prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    ApplyLayout(hwnd, nNewDpi);
    SetProp(hwnd, DPI_PROP, (HANDLE)(UINT_PTR)nNewDpi);
    return TRUE;
}
