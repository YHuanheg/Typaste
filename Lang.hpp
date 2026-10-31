// Lang.hpp --- runtime language selection for Typaste
// Copyright (C) 2019-2024 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
#pragma once

#include <windows.h>
#include <string>

// The registry key of the settings
#define TYP_REGKEY L"SOFTWARE\\Katayama Hirofumi MZ\\Typaste"

// The language choices that are shown in the settings dialog.
// The values are stored in the registry as the "Language" value in tag form.
enum TYPASTE_LANG
{
    TYPASTE_LANG_AUTO = 0,  // follow the UI language of the system
    TYPASTE_LANG_EN_US,     // 0x0409
    TYPASTE_LANG_JA_JP,     // 0x0411
    TYPASTE_LANG_ZH_CN,     // 0x0804
    TYPASTE_LANG_ZH_TW,     // 0x0404
    TYPASTE_LANG_COUNT
};

// The language resource that is actually in use
extern LANGID g_langid;

// Initialize the language subsystem (reads the registry value and resolves "auto")
void Lang_Init(HINSTANCE hInst);

// The current setting (one of TYPASTE_LANG_*)
int Lang_GetSetting(void);

// Change the language setting (the caller is responsible for saving it)
void Lang_SetSetting(int setting);

// Resolve a setting into a LANGID
LANGID Lang_ResolveLangID(int setting);

// The BCP-47 tag of a setting (L"auto" for TYPASTE_LANG_AUTO)
const wchar_t *Lang_GetTag(int setting);

// Convert a tag into a setting; returns TYPASTE_LANG_AUTO for unknown tags
int Lang_GetSettingFromTag(const wchar_t *tag);

// Load a string from the localized STRINGTABLE
std::wstring Lang_LoadString(UINT id);

// Create a dialog from the localized dialog template
HWND Lang_CreateDialog(HINSTANCE hInst, LPCWSTR lpTemplateName, HWND hWndParent,
                       DLGPROC lpDialogFunc, LPARAM dwInitParam);
INT_PTR Lang_DialogBox(HINSTANCE hInst, LPCWSTR lpTemplateName, HWND hWndParent,
                       DLGPROC lpDialogFunc, LPARAM dwInitParam);
