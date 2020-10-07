/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "../FileUtils.h"
#include "p8-platform/windows/CharsetConverter.h"
#include <string>
#include "../utils.h"
#ifdef TARGET_WINDOWS_DESKTOP
#include <Shlobj.h>
#endif

#ifdef TARGET_WINDOWS
#include <windows.h>
#include <fileapi.h>
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
