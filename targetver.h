// targetver.h --- target versioning header for Win             -*- C++ -*-
//////////////////////////////////////////////////////////////////////////////
//
// This header defines the Win32API macros that target the version of Windows
// that Typaste supports. Every value is guarded with #ifndef so that it can be
// overridden from the command line, e.g. -DWINVER=0x0601.
//
// The x64 build targets Windows Vista or later. The legacy Win32 build keeps
// the same minimum version.
//
// Old targets, for reference:
//   Win95     WINVER 0x0400  _WIN32_IE 0x0300
//   Win98     WINVER 0x0410  _WIN32_IE 0x0401
//   WinNT4    WINVER 0x0400  _WIN32_IE 0x0200
//   WinME     WINVER 0x0500  _WIN32_IE 0x0500
//   Win2000   WINVER 0x0500  _WIN32_IE 0x0500
//   WinXP     WINVER 0x0501  _WIN32_IE 0x0501
//   WinVista  WINVER 0x0600  _WIN32_IE 0x0600
//   Win7      WINVER 0x0601  _WIN32_IE 0x0601
//
//////////////////////////////////////////////////////////////////////////////

#ifndef WINVER
    #define WINVER          0x0600      // Windows Vista
#endif
#ifndef _WIN32_WINNT
    #define _WIN32_WINNT    0x0600      // Windows Vista
#endif
#ifndef _WIN32_IE
    #define _WIN32_IE       0x0600
#endif
#ifndef NTDDI_VERSION
    #define NTDDI_VERSION   0x06000000
#endif

// Users of Visual Studio 10+ can do this instead
// #include "SDKDDKVer.h"

//////////////////////////////////////////////////////////////////////////////
