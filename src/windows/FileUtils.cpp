/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "../FileUtils.h"
#include <string>
#include "../utils.h"
#ifdef TARGET_WINDOWS_DESKTOP
#include <Shlobj.h>
#endif

#include <windows.h>
#include <fileapi.h>

std::wstring ToW(const char* str, size_t length)
{
  int result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, length, nullptr, 0);
  if (result == 0)
    return std::wstring();

  auto newStr = std::make_unique<wchar_t[]>(result);
  result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, length, newStr.get(), result);

  if (result == 0)
    return std::wstring();

  return std::wstring(newStr.get(), result);
}

namespace OS
{
  bool CFile::Exists(const std::string& strFileName, long* errCode)
  {
    std::string strWinFile = ToWindowsPath(strFileName);
    std::wstring strWFile = ToW(strWinFile.c_str(), 0);
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
