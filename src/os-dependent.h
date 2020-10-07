/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

#if (defined(_WIN32) || defined(_WIN64))

#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef _W64 int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

#else

#if (defined(TARGET_LINUX) || defined(TARGET_DARWIN))
#include <sys/types.h>
#include <chrono>
#include <cstring>
inline unsigned long long GetTickCount64(void)
{
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
};
#endif /* TARGET_LINUX || TARGET_DARWIN */

#endif

// Additional typedefs
typedef uint8_t byte;
