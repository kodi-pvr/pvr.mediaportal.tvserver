/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <stdlib.h>
#include "Cards.h"
#include "GenreTable.h"
#include "DateTime.h"
#include "channels.h"

#include <kodi/addon-instance/pvr/Recordings.h>

#define DEFAULTFRAMESPERSECOND 25.0
#define MAXPRIORITY 99
#define MAXLIFETIME 99 //Based on VDR addon and VDR documentation. 99=Keep forever, 0=can be deleted at any time, 1..98=days to keep

class cRecording
{
private:
  int m_Index;
  int m_channelID;
  std::string m_channelName;
  std::string m_filePath;          ///< The full recording path as returned by the backend
  std::string m_basePath;          ///< The base path shared by all recordings (to be determined from the Card settings)
  std::string m_directory;         ///< An optional subdirectory below the basePath
  std::string m_fileName;          ///< The recording filename without path
  std::string m_stream;
  std::string m_originalurl;
  MPTV::CDateTime m_startTime;
  MPTV::CDateTime m_endTime;
  int m_duration;                  ///< Duration of the recording in seconds
  std::string m_title;             ///< Title of this event
  std::string m_description;       ///< Description of this event
  std::string m_episodeName;       ///< Short description of this event (typically the episode name in case of a series)
  std::string m_seriesNumber;
  std::string m_episodeNumber;
  std::string m_episodePart;
  int m_scheduleID;
  int m_keepUntil;
  MPTV::CDateTime m_keepUntilDate; ///< MediaPortal keepUntilDate
  CCards* m_cardSettings;          ///< Pointer to the MediaPortal card settings. Will be used to determine the base path of the recordings
  std::string m_genre;
  int m_genre_type;
  int m_genre_subtype;
  bool m_isRecording;
  CGenreTable* m_genretable;
  int m_timesWatched;
  int m_lastPlayedPosition;
  int m_channelType;

public:
  cRecording();
  virtual ~cRecording();

  bool ParseLine(const std::string& data);
  const char *ChannelName(void) const { return m_channelName.c_str(); }
  int Index(void) const { return m_Index; }
  time_t StartTime(void) const;
  int Duration(void) const;
  const char *Title(void) const { return m_title.c_str(); }
  const char *Description(void) const { return m_description.c_str(); }
  const char *EpisodeName(void) const { return m_episodeName.c_str(); }
  const char *SeriesNumber(void) const { return m_seriesNumber.c_str(); }
  const char *EpisodeNumber(void) const { return m_episodeNumber.c_str(); }
  const char *EpisodePart(void) const { return m_episodePart.c_str(); }
  int ScheduleID(void) const { return m_scheduleID; }
  int Lifetime(void) const;
  int TimesWatched(void) const {return m_timesWatched; }
  int LastPlayedPosition(void) const { return m_lastPlayedPosition; }
  bool IsRecording(void) {return m_isRecording; }
  int ChannelID(void) const { return m_channelID; }
  PVR_RECORDING_CHANNEL_TYPE GetChannelType(void) const;

  /**
   * \brief Filename of this recording with full path (at server side)
   */
  const char *FilePath(void) const { return m_filePath.c_str(); }

  /**
   * \brief Filename of this recording without full path
   * \return Filename
   */
  const char *FileName(void) const { return m_fileName.c_str(); }

  /**
   * \brief Directory where this recording is stored (at server side)
   * \return Filename
   */
  const char *Directory(void) const { return m_directory.c_str(); }

  /**
   * \brief Override the directory where this recording is stored
   */
  //void SetDirectory( std::string& directory );

  /**
   * \brief The RTSP stream URL for this recording (hostname resolved to IP-address)
   * \return Stream URL
   */
  const char *Stream(void) const { return m_stream.c_str(); }

  /**
   * \brief The RTSP stream URL for this recording (unresolved hostname)
   * \return Stream URL
   */
  const char *OrignalURL(void) const { return m_originalurl.c_str(); }

  /**
   * \brief Pass a pointer to the MediaPortal card settings to this class
   * \param the cardSettings
   */
  void SetCardSettings(CCards* cardSettings);

  /**
   * \brief Parse Recording file path and divide it in 3 parts: base path, subdirectory and filename;
   */
  void SplitFilePath(void);

  int GenreType(void) const { return m_genre_type; }
  int GenreSubType(void) const { return m_genre_subtype; }
  void SetGenreTable(CGenreTable* genremap);
  const char* GetGenre(void) const { return m_genre.c_str(); }

  /**
   * \brief Returns the series number as an integer value. Returns -1 when this field is empty.
   */
  int GetSeriesNumber(void) const;

  /**
  * \brief Returns the episode number as an integer value. Returns -1 when this field is empty.
  */
  int GetEpisodeNumber(void) const;
};
