/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

namespace TvDatabase
{
  // From MediaPortal: TvDatabase.ChannelType
  namespace ChannelType
  {
    const int Unknown = -1; //Added
    const int Tv = 0;
    const int Radio = 1;
    const int Web = 2;
    const int All = 3;
  };
}

class cChannel
{
private:
  std::string name;
  int uid;
  int external_id;
  bool encrypted;
  bool iswebstream;
  bool visibleinguide;
  std::string url;
  int majorChannelNr;
  int minorChannelNr;

public:
  cChannel();
  virtual ~cChannel();

  bool Parse(const std::string& data);
  const char *Name(void) const { return name.c_str(); }
  int UID(void) const { return uid; }
  int ExternalID(void) const { return external_id; }
  bool Encrypted(void) const { return encrypted; }
  bool IsWebstream(void) const { return iswebstream; }
  bool VisibleInGuide(void) const { return visibleinguide; }
  const char* URL(void) const { return url.c_str(); }
  int MajorChannelNr(void) const { return majorChannelNr; }
  int MinorChannelNr(void) const { return minorChannelNr; }
};

