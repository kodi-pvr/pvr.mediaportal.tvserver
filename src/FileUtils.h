/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

namespace OS
{
  class CFile
  {
  public:
    static bool Exists(const std::string& strFileName, long* errCode = NULL);
  };

#ifdef TARGET_WINDOWS_DESKTOP
  /**
   * Return the location of the Program Data folder
   * @param[in,out] programData Reference to a string that will receive the program data path
   * @return true on success, false on failure
   */
  bool GetProgramData(std::string& programData);
#endif
};
