/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#ifdef TARGET_WINDOWS
#pragma warning(disable:4244) //wchar to char = loss of data
#endif

#include "utils.h"
#include "settings.h"
#include <string>
#include <stdio.h>
#include <kodi/tools/StringUtils.h>

using namespace kodi::tools;
using namespace std;

void Tokenize(const string& str, vector<string>& tokens, const string& delimiters = " ")
{
  // Skip delimiters at beginning.
  //string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  // Don't skip delimiters at beginning.
  string::size_type start_pos = 0;
  // Find first "non-delimiter".
  string::size_type delim_pos = 0;

  while (string::npos != delim_pos)
  {
    delim_pos = str.find_first_of(delimiters, start_pos);
    // Found a token, add it to the vector.
    tokens.push_back(str.substr(start_pos, delim_pos - start_pos));
    start_pos = delim_pos + 1;

    // Find next "non-delimiter"
  }
}


std::string WStringToString(const std::wstring& s)
{
  std::string temp(s.length(), ' ');
  std::copy(s.begin(), s.end(), temp.begin());
  return temp;
}

std::wstring StringToWString(const std::string& s)
{
  std::wstring temp(s.length(),L' ');
  std::copy(s.begin(), s.end(), temp.begin());
  return temp;
}

std::string lowercase(const std::string& s)
{
  std::string t;
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i)
  {
    t += tolower(*i);
  }
  return t;
}

bool stringtobool(const std::string& s)
{
  std::string temp = lowercase(s);

  if(temp.compare("false") == 0)
    return false;
  else if(temp.compare("0") == 0)
    return false;
  else
    return true;
}

const char* booltostring(const bool b)
{
  return (b==true) ? "True" : "False";
}

std::string ToThumbFileName(const char* strChannelName)
{
  std::string strThumbName = strChannelName;

  StringUtils::Replace(strThumbName, ":","_");
  StringUtils::Replace(strThumbName, "/","_");
  StringUtils::Replace(strThumbName, "\\","_");
  StringUtils::Replace(strThumbName, ">","_");
  StringUtils::Replace(strThumbName, "<","_");
  StringUtils::Replace(strThumbName, "*","_");
  StringUtils::Replace(strThumbName, "?","_");
  StringUtils::Replace(strThumbName, "\"","_");
  StringUtils::Replace(strThumbName, "|","_");

  return strThumbName;
}

std::string ToKodiPath(const std::string& strFileName)
{
  std::string strKodiFileName(strFileName);

  if (StringUtils::Left(strKodiFileName, 2) == "\\\\")
  {
    std::string SMBPrefix = "smb://";

    if (!CSettings::Get().GetSMBusername().empty())
    {
      SMBPrefix += CSettings::Get().GetSMBusername();
      if (!CSettings::Get().GetSMBpassword().empty())
      {
        SMBPrefix += ":" + CSettings::Get().GetSMBpassword();
      }
      SMBPrefix += "@";
    }
    StringUtils::Replace(strKodiFileName, "\\\\", SMBPrefix.c_str());
    StringUtils::Replace(strKodiFileName, '\\', '/');
  }

  return strKodiFileName;
}

std::string ToWindowsPath(const std::string& strFileName)
{
  std::string strWinFileName;
  std::size_t found = strFileName.find_first_of('@');

  if (found != std::string::npos)
  {
    strWinFileName = "\\\\" + strFileName.substr(found+1);
  }
  else
  {
    strWinFileName = strFileName;
    StringUtils::Replace(strWinFileName, "smb://","\\\\");
  }

  StringUtils::Replace(strWinFileName, '/','\\');

  return strWinFileName;
}

//////////////////////////////////////////////////////////////////////////////
