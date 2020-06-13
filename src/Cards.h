/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <vector>
#include <string>
#include "DateTime.h"


/**
 * MediaPortal TVServer card settings ("card" table in the database)
 */
typedef struct Card
{
  int       IdCard;
  std::string    DevicePath;
  std::string    Name;
  int       Priority;
  bool      GrabEPG;
  MPTV::CDateTime LastEpgGrab;
  std::string    RecordingFolder;
  std::string    RecordingFolderUNC;
  int       IdServer;
  bool      Enabled;
  int       CamType;
  std::string    TimeshiftFolder;
  std::string    TimeshiftFolderUNC;
  int       RecordingFormat;
  int       DecryptLimit;
  bool      Preload;
  bool      CAM;
  int       NetProvider;
  bool      StopGraph;
} Card;

class CCards: public std::vector<Card>
{
  public:

    /**
     * \brief Parse the multi-line string response from the TVServerKodi plugin command "GetCardSettings"
     * The data is stored in "struct Card" item.
     *
     * \param lines Vector with response lines
     * \return True on success, False on failure
     */
    bool ParseLines(std::vector<std::string>& lines);

    /**
     * \brief Return the data for the card with the given id
     * \param id The card id
     * \param card Return value: card data or NULL if not found.
     * \return True on success, False on failure
     */
    bool GetCard(int id, Card& card);
};
