/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "WindowsUtils.h"
#include <windows.h>
#include <stdio.h>
#include <string>

namespace OS
{
  bool GetEnvironmentVariable(const char* strVarName, std::string& strResult)
  {
#ifdef TARGET_WINDOWS_DESKTOP
    char strBuffer[4096];
    DWORD dwRet;

    dwRet = ::GetEnvironmentVariableA(strVarName, strBuffer, 4096);

    if(0 == dwRet)
    {
      dwRet = GetLastError();
      if( ERROR_ENVVAR_NOT_FOUND == dwRet )
      {
        strResult.clear();
        return false;
      }
    }
    strResult = strBuffer;
    return true;
#else
    return false;
#endif // TARGET_WINDOWS_DESKTOP
  }
}
