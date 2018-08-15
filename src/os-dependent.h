#pragma once
/*
 *      Copyright (C) 2005-2012 Team Kodi
 *      https://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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
