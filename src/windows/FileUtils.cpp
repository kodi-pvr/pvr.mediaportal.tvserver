/*
 *      Copyright (C) 2005-2014 Team Kodi
 *      https://kodi.tv
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

#include "../FileUtils.h"
#include "p8-platform/os.h"
#include "p8-platform/windows/CharsetConverter.h"
#include <string>
#include "../utils.h"
#ifdef TARGET_WINDOWS_DESKTOP
#include <Shlobj.h>
#endif

namespace OS
{
  bool CFile::Exists(const std::string& strFileName, long* errCode)
  {
    std::string strWinFile = ToWindowsPath(strFileName);
    std::wstring strWFile = p8::windows::ToW(strWinFile.c_str());
    DWORD dwAttr = GetFileAttributesW(strWFile.c_str());

    if(dwAttr != 0xffffffff)
    {
      return true;
    }

    if (errCode)
      *errCode = GetLastError();

    return false;
  }

#ifdef TARGET_WINDOWS_DESKTOP
  /**
  * Return the location of the Program Data folder
  */
  bool GetProgramData(std::string& programData)
  {
    LPWSTR wszPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &wszPath) != S_OK)
    {
      return false;
    }
    std::wstring wPath = wszPath;
    CoTaskMemFree(wszPath);
    programData = WStringToString(wPath);

    return true;
  }

#endif
}
