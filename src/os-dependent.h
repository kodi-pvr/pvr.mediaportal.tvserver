/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "p8-platform/os.h"

#ifdef TARGET_LINUX
// Retrieve the number of milliseconds that have elapsed since the system was started
#include <time.h>
inline unsigned long long GetTickCount64(void)
{
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
  {
    return 0;
  }
  return (unsigned long long)( (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000) );
};
#elif defined(TARGET_DARWIN)
#include <time.h>
inline unsigned long long GetTickCount64(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long long)( (tv.tv_sec * 1000) + (tv.tv_usec / 1000) );
};
#endif /* TARGET_LINUX || TARGET_DARWIN */

// Additional typedefs
typedef uint8_t byte;
