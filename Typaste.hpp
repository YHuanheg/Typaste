// Typaste.hpp --- the common header of Typaste
// Copyright (C) 2019-2024 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// Copyright (C) 2026 YHuanheg
// This file is public domain software.
#include "targetver.h"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>
#include <cstdlib>
#include <string>
#include "AutoType.hpp"
#include "Lang.hpp"
#include "resource.h"

// Backward compatibility for BCC55
#ifndef _countof
    #define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

// The generic string class
#ifdef UNICODE
    typedef std::wstring tstring_t;
#else
    typedef std::string tstring_t;
#endif

// The generic text clipboard format
#ifdef UNICODE
    #define CF_GENERICTEXT CF_UNICODETEXT
#else
    #define CF_GENERICTEXT CF_TEXT
#endif
