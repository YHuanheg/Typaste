// Lang.cpp --- runtime language selection for Typaste
// Copyright (C) 2019-2024 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
//
// Win32 has no way to ask "give me the Japanese string" through LoadString().
// LoadString() always follows the UI language of the thread, which is why this
// module locates the resources by itself with FindResourceEx() and then builds
// the dialog or the string out of the raw resource data. That makes the language
// an explicit, user selectable property of the application.
#include "Lang.hpp"
#include "resource.h"
#include <cwchar>

extern HINSTANCE g_hInst;

LANGID g_langid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

static int s_nLangSetting = TYPASTE_LANG_AUTO;

// The resource types as wide strings (RT_STRING and RT_DIALOG are MAKEINTRESOURCE macros)
static const LPCWSTR kRTString = MAKEINTRESOURCEW(6); // RT_STRING
static const LPCWSTR kRTDialog = MAKEINTRESOURCEW(5); // RT_DIALOG

LANGID Lang_ResolveLangID(int setting)
{
    switch (setting)
    {
    case TYPASTE_LANG_EN_US:
        return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    case TYPASTE_LANG_JA_JP:
        return MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    case TYPASTE_LANG_ZH_CN:
        return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    case TYPASTE_LANG_ZH_TW:
        return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
    default:
        break;
    }

    // "auto": follow the UI language of the system
    LANGID ui = ::GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(ui))
    {
    case LANG_JAPANESE:
        return MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    case LANG_CHINESE:
        switch (SUBLANGID(ui))
        {
        case SUBLANG_CHINESE_TRADITIONAL:
#ifdef SUBLANG_CHINESE_HONGKONG
        case SUBLANG_CHINESE_HONGKONG:
#endif
#ifdef SUBLANG_CHINESE_MACAU
        case SUBLANG_CHINESE_MACAU:
#endif
            return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
        default:
            return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
        }
    default:
        return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    }
}

const wchar_t *Lang_GetTag(int setting)
{
    switch (setting)
    {
    case TYPASTE_LANG_EN_US: return L"en-US";
    case TYPASTE_LANG_JA_JP: return L"ja-JP";
    case TYPASTE_LANG_ZH_CN: return L"zh-CN";
    case TYPASTE_LANG_ZH_TW: return L"zh-TW";
    default: return L"auto";
    }
}

int Lang_GetSettingFromTag(const wchar_t *tag)
{
    if (!tag || !*tag)
        return TYPASTE_LANG_AUTO;

    static const struct { const wchar_t *tag; int setting; } table[] =
    {
        { L"auto",   TYPASTE_LANG_AUTO },
        { L"en-US",  TYPASTE_LANG_EN_US },
        { L"en",     TYPASTE_LANG_EN_US },
        { L"ja-JP",  TYPASTE_LANG_JA_JP },
        { L"ja",     TYPASTE_LANG_JA_JP },
        { L"zh-CN",  TYPASTE_LANG_ZH_CN },
        { L"zh-Hans", TYPASTE_LANG_ZH_CN },
        { L"zh",     TYPASTE_LANG_ZH_CN },
        { L"zh-TW",  TYPASTE_LANG_ZH_TW },
        { L"zh-Hant", TYPASTE_LANG_ZH_TW },
        { L"zh-HK",  TYPASTE_LANG_ZH_TW },
    };

    for (const auto& entry : table)
    {
        if (_wcsicmp(tag, entry.tag) == 0)
            return entry.setting;
    }
    return TYPASTE_LANG_AUTO;
}

static void Lang_ReadRegSetting(int *pSetting)
{
    *pSetting = TYPASTE_LANG_AUTO;

    HKEY hKey = NULL;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, TYP_REGKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    wchar_t szTag[64] = { 0 };
    DWORD dwType = 0;
    DWORD cbData = sizeof(szTag) - sizeof(wchar_t);
    LONG error = ::RegQueryValueExW(hKey, L"Language", NULL, &dwType, (LPBYTE)szTag, &cbData);
    ::RegCloseKey(hKey);

    if (error != ERROR_SUCCESS || dwType != REG_SZ)
        return;

    szTag[_countof(szTag) - 1] = 0;
    *pSetting = Lang_GetSettingFromTag(szTag);
}

void Lang_Init(HINSTANCE hInst)
{
    UNREFERENCED_PARAMETER(hInst);

    int setting = TYPASTE_LANG_AUTO;
    Lang_ReadRegSetting(&setting);
    Lang_SetSetting(setting);
}

int Lang_GetSetting(void)
{
    return s_nLangSetting;
}

void Lang_SetSetting(int setting)
{
    if (setting <= TYPASTE_LANG_AUTO || setting >= TYPASTE_LANG_COUNT)
        setting = TYPASTE_LANG_AUTO;

    s_nLangSetting = setting;
    g_langid = Lang_ResolveLangID(setting);

    // Let the loader fall back the same way for the resources that this module
    // does not intercept (icons, manifest, version info).
    ::SetThreadUILanguage(g_langid);
}

// Find a resource: the selected language first, then the neutral one
static HRSRC FindLocalizedResource(LPCWSTR lpType, LPCWSTR lpName)
{
    HRSRC hRes = ::FindResourceExW(g_hInst, lpType, lpName, g_langid);
    if (!hRes)
        hRes = ::FindResourceExW(g_hInst, lpType, lpName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    if (!hRes)
        hRes = ::FindResourceExW(g_hInst, lpType, lpName, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    if (!hRes)
        hRes = ::FindResourceW(g_hInst, lpName, lpType);
    return hRes;
}

std::wstring Lang_LoadString(UINT id)
{
    std::wstring str;

    HRSRC hRes = FindLocalizedResource(kRTString, MAKEINTRESOURCEW(id / 16 + 1));
    if (!hRes)
        return str;

    DWORD dwSize = (DWORD)::SizeofResource(g_hInst, hRes);
    HGLOBAL hGlob = ::LoadResource(g_hInst, hRes);
    if (!hGlob || dwSize < sizeof(WORD))
        return str;

    const BYTE *pb = (const BYTE *)::LockResource(hGlob);
    if (!pb)
        return str;

    const BYTE *pbEnd = pb + dwSize;

    // A STRINGTABLE block holds 16 entries. Each entry is:
    //   WORD cch; WCHAR sz[cch];
    for (UINT i = 0; i < id % 16; ++i)
    {
        if (pb + sizeof(WORD) > pbEnd)
            return str;
        WORD cch = *(const WORD *)pb;
        pb += sizeof(WORD) + (size_t)cch * sizeof(WCHAR);
        if (pb > pbEnd)
            return str;
    }

    if (pb + sizeof(WORD) > pbEnd)
        return str;

    WORD cch = *(const WORD *)pb;
    pb += sizeof(WORD);
    if (pb + (size_t)cch * sizeof(WCHAR) > pbEnd)
        return str;

    str.assign((const WCHAR *)pb, cch);
    return str;
}

static const DLGTEMPLATE *LoadDialogTemplate(LPCWSTR lpTemplateName)
{
    HRSRC hRes = FindLocalizedResource(kRTDialog, lpTemplateName);
    if (!hRes)
        return NULL;

    HGLOBAL hGlob = ::LoadResource(g_hInst, hRes);
    if (!hGlob)
        return NULL;

    return (const DLGTEMPLATE *)::LockResource(hGlob);
}

HWND Lang_CreateDialog(HINSTANCE hInst, LPCWSTR lpTemplateName, HWND hWndParent,
                       DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    const DLGTEMPLATE *pTemplate = LoadDialogTemplate(lpTemplateName);
    if (!pTemplate)
        return NULL;

    return ::CreateDialogIndirectParamW(hInst, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

INT_PTR Lang_DialogBox(HINSTANCE hInst, LPCWSTR lpTemplateName, HWND hWndParent,
                       DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    const DLGTEMPLATE *pTemplate = LoadDialogTemplate(lpTemplateName);
    if (!pTemplate)
        return -1;

    return ::DialogBoxIndirectParamW(hInst, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}
