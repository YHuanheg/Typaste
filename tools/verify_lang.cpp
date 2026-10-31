// verify_lang.cpp --- check that all language resources are present in Typaste.exe
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
//
//   verify_lang.exe <path to Typaste.exe>
//
// It opens the executable as a data file and looks up the dialog templates and
// the strings of every supported language by its exact LANGID. Exit code 0 means
// that everything was found.
#include <windows.h>
#include <stdio.h>
#include <string>

static std::wstring LoadStringEx(HMODULE h, UINT id, LANGID lang)
{
    std::wstring str;

    HRSRC hRes = FindResourceExW(h, MAKEINTRESOURCEW(6), MAKEINTRESOURCEW(id / 16 + 1), lang);
    if (!hRes)
        return str;

    DWORD dwSize = SizeofResource(h, hRes);
    HGLOBAL hGlob = LoadResource(h, hRes);
    if (!hGlob || dwSize < sizeof(WORD))
        return str;

    const BYTE *pb = (const BYTE *)LockResource(hGlob);
    if (!pb)
        return str;
    const BYTE *pbEnd = pb + dwSize;

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

    return std::wstring((const WCHAR *)pb, cch);
}

static void Put(const std::wstring& s)
{
    char buf[2048];
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), buf, sizeof(buf) - 1, NULL, NULL);
    if (n < 0)
        n = 0;
    buf[n] = 0;
    fputs(buf, stdout);
}

int wmain(int argc, wchar_t **argv)
{
    SetConsoleOutputCP(CP_UTF8);

    const wchar_t *pszExe = (argc > 1) ? argv[1] : L"Typaste.exe";
    HMODULE h = LoadLibraryExW(pszExe, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!h)
    {
        printf("ERROR: cannot open %ls (error %lu)\n", pszExe, GetLastError());
        return 2;
    }

    struct LANGENTRY { LANGID id; const wchar_t *name; };
    static const LANGENTRY langs[] = {
        { MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), L"en-US" },
        { MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), L"ja-JP" },
        { MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), L"zh-CN" },
        { MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL), L"zh-TW" },
    };

    int failed = 0;
    for (int i = 0; i < (int)(sizeof(langs) / sizeof(langs[0])); ++i)
    {
        LANGID lang = langs[i].id;
        BOOL bMain = (FindResourceExW(h, MAKEINTRESOURCEW(5), MAKEINTRESOURCEW(100), lang) != NULL);
        BOOL bSettings = (FindResourceExW(h, MAKEINTRESOURCEW(5), MAKEINTRESOURCEW(101), lang) != NULL);
        std::wstring title = LoadStringEx(h, 102, lang);   // IDS_TITLETEXT
        std::wstring autoText = LoadStringEx(h, 108, lang); // IDS_AUTOLANG

        printf("[%ls] dialogs=%s/%s title=", langs[i].name,
               bMain ? "ok" : "MISSING", bSettings ? "ok" : "MISSING");
        Put(title);
        printf(" auto=");
        Put(autoText);
        printf("\n");

        if (!bMain || !bSettings || title.empty() || autoText.empty())
            failed = 1;
    }

    if (failed)
    {
        printf("RESULT: FAILED\n");
        return 1;
    }
    printf("RESULT: OK\n");
    return 0;
}
