/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

#if (defined(_WIN32) || defined(_WIN64))

#include <wchar.h>

/* Handling of 2-byte Windows wchar strings */
#define WcsLen wcslen
#define WcsToMbs wcstombs
typedef wchar_t Wchar_t; /* sizeof(wchar_t) = 2 bytes on Windows */

#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef _W64 int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

/* Prevent deprecation warnings */
#define strnicmp _strnicmp

#define PATH_SEPARATOR_CHAR '\\'

#else

#if (defined(TARGET_LINUX) || defined(TARGET_DARWIN))
#include <sys/types.h>
#include <chrono>
#include <cstring>

#define strnicmp(X,Y,N) strncasecmp(X,Y,N)

inline unsigned long long GetTickCount64(void)
{
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
};

#define PATH_SEPARATOR_CHAR '/'

#if defined(__APPLE__)
// for HRESULT
#include <CoreFoundation/CFPlugInCOM.h>
#endif

/* Handling of 2-byte Windows wchar strings on non-Windows targets
 * Used by The MediaPortal and ForTheRecord pvr addons
 */
typedef uint16_t Wchar_t; /* sizeof(wchar_t) = 4 bytes on Linux, but the MediaPortal buffer files have 2-byte wchars */

/* This is a replacement of the Windows wcslen() function which assumes that
 * wchar_t is a 2-byte character.
 * It is used for processing Windows wchar strings
 */
inline size_t WcsLen(const Wchar_t *str)
{
  const unsigned short *eos = (const unsigned short*)str;
  while( *eos++ ) ;
  return( (size_t)(eos - (const unsigned short*)str) -1);
};

/* This is a replacement of the Windows wcstombs() function which assumes that
 * wchar_t is a 2-byte character.
 * It is used for processing Windows wchar strings
 */
inline size_t WcsToMbs(char *s, const Wchar_t *w, size_t n)
{
  size_t i = 0;
  const unsigned short *wc = (const unsigned short*) w;
  while(wc[i] && (i < n))
  {
    s[i] = wc[i];
    ++i;
  }
  if (i < n) s[i] = '\0';

  return (i);
};

#endif /* TARGET_LINUX || TARGET_DARWIN */

#endif

typedef long LONG;
#if !defined(__APPLE__)
typedef LONG HRESULT;
#endif

#ifndef FAILED
#define FAILED(Status) ((HRESULT)(Status)<0)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

#define _FILE_OFFSET_BITS 64
#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2

#ifndef S_OK
#define S_OK           0L
#endif

#ifndef S_FALSE
#define S_FALSE        1L
#endif

// Error codes
#define ERROR_FILENAME_EXCED_RANGE 206L
#define ERROR_INVALID_NAME         123L

#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY              0x8007000EL
#endif

#ifndef E_FAIL
#define E_FAIL                     0x8004005EL
#endif

// Additional typedefs
typedef uint8_t byte;
