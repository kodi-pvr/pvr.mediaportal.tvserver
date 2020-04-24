/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <vector>
#include <ctime>
#include "uri.h"
#include "p8-platform/util/util.h"

#ifdef TARGET_WINDOWS
#include "windows/WindowsUtils.h"
#endif


/**
 * String tokenize
 * Split string using the given delimiter into a vector of substrings
 */
void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);

std::wstring StringToWString(const std::string& s);
std::string WStringToString(const std::wstring& s);
std::string lowercase(const std::string& s);
bool stringtobool(const std::string& s);
const char* booltostring(const bool b);

/**
 * @brief Filters forbidden filename characters from channel name and replaces them with _ )
 */
std::string ToThumbFileName(const char* strChannelName);

std::string ToKodiPath(const std::string& strFileName);
std::string ToWindowsPath(const std::string& strFileName);

/**
 * @brief Macro to silence unused parameter warnings
 */
#ifdef UNUSED
#  undef UNUSED
#endif
#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x)  /* x */
#endif
