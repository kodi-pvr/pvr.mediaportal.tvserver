/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <vector>
#include "channels.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

cChannel::cChannel()
{
  uid = 0;
  external_id = 0;
  iswebstream = false;
  encrypted = false;
  visibleinguide = true;
  majorChannelNr = -1;
  minorChannelNr = -1;
}

cChannel::~cChannel()
{
}

bool cChannel::Parse(const std::string& data)
{
  std::vector<std::string> fields;

  Tokenize(data, fields, "|");

  if (fields.size() >= 4)
  {
    // Expected format:
    // ListTVChannels, ListRadioChannels
    // 0 = channel uid
    // 1 = channel external id/number
    // 2 = channel name
    // 3 = isencrypted ("0"/"1")
    // ListRadioChannels only: (TVServerKodi >= v1.1.0.100)
    // 4 = iswebstream
    // 5 = webstream url
    // 6 = visibleinguide (TVServerKodi >= v1.2.3.120)
    // 7 = ATSC major channel number (TVServerKodi >= v1.8.0.126)
    // 8 = ATSC minor channel number (TVServerKodi >= v1.8.0.126)

    uid = atoi(fields[0].c_str());
    external_id = atoi(fields[1].c_str());
    name = fields[2];
    encrypted = (strncmp(fields[3].c_str(), "1", 1) == 0);

    if (fields.size() >= 6)
    {
      iswebstream = (strncmp(fields[4].c_str(), "1", 1) == 0);
      url = fields[5].c_str();

      if (fields.size() >= 7)
      {
        visibleinguide = (strncmp(fields[6].c_str(), "1", 1) == 0);
        if (fields.size() >= 9)
        {
          majorChannelNr = atoi(fields[7].c_str());
          minorChannelNr = atoi(fields[8].c_str());
        }
        else
        {
          majorChannelNr = -1;
          minorChannelNr = -1;
        }
      }
    }

    return true;
  }
  else
  {
    return false;
  }
}
