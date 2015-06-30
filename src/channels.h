#pragma once
/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1335  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "libXBMC_pvr.h"
#include <string>

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

