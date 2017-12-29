/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
