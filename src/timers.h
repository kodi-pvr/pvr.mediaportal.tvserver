#pragma once
/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#ifndef __TIMERS_H
#define __TIMERS_H

#include "libXBMC_pvr.h"
#include <stdlib.h>
#include <string>
#include "DateTime.h"
#include "GenreTable.h"

namespace TvDatabase
{

// From MediaPortal: TvDatabase.ScheduleRecordingType
enum ScheduleRecordingType
{                                  // English MediaPortal GUI description:
  Once = 0,                        // Record once
  Daily = 1,                       // Record every day at this time
  Weekly = 2,                      // Record every week at this time
  EveryTimeOnThisChannel = 3,      // Record every time on this channel
  EveryTimeOnEveryChannel = 4,     // Record every time on every channel
  Weekends = 5,                    // Record Weekends
  WorkingDays = 6,                 // Record Weekdays
  WeeklyEveryTimeOnThisChannel = 7, // Weekly on this channel
  KodiManual = 99                   // Special type for Kodi since it distinguishes between 'Once (EPG based)' and 'manual'
};

// From MediaPortal: TvDatabase.KeepMethodType
enum KeepMethodType
{
  UntilSpaceNeeded = 0,
  UntilWatched = 1,
  TillDate = 2,
  Always = 3
};

};

const int cSecsInDay  = 86400;          ///> Amount of seconds in one day
const int cKodiTimerTypeOffset = (PVR_TIMER_TYPE_NONE + 1);        ///> Offset used to map the ScheduleRecordingType onto the iTimerType values
const int cKodiTimerIndexOffset = (PVR_TIMER_NO_CLIENT_INDEX + 1); ///> Offset used to map the MediaPortal schedule id's to the iClientIndex values

#define MPTV_REPEAT_NO_SERIES_OFFSET 0x7FFFFFF
#define MPTV_NO_PARENT_SCHEDULE -1

// Kodi Keep methods :
// The defines below are uses as replacement for the MediaPortal Until date version since
// we cannot have both data selection and the three above keep methods at the same time
#define MPTV_KEEP_ONE_WEEK              7
#define MPTV_KEEP_TWO_WEEKS            14
#define MPTV_KEEP_THREE_WEEKS          21
#define MPTV_KEEP_ONE_MONTH            31
#define MPTV_KEEP_TWO_MONTHS           61
#define MPTV_KEEP_THREE_MONTHS         92
#define MPTV_KEEP_FOUR_MONTHS         122
#define MPTV_KEEP_FIVE_MONTHS         153
#define MPTV_KEEP_SIX_MONTHS          183
#define MPTV_KEEP_SEVEN_MONTHS        214
#define MPTV_KEEP_EIGHT_MONTHS        244
#define MPTV_KEEP_NINE_MONTHS         275
#define MPTV_KEEP_TEN_MONTHS          305
#define MPTV_KEEP_ELEVEN_MONTHS       336
#define MPTV_KEEP_ONE_YEAR            365

#define MPTV_KEEP_UNTIL_SPACE_NEEDED TvDatabase::UntilSpaceNeeded
#define MPTV_KEEP_UNTIL_WATCHED      TvDatabase::UntilWatched
#define MPTV_KEEP_ALWAYS             TvDatabase::Always

// Kodi timer types
const int MPTV_RECORD_ONCE =
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_EVERY_TIME_ON_THIS_CHANNEL =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_EVERY_TIME_ON_EVERY_CHANNEL =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_WEEKLY =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_DAILY =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_WORKING_DAYS =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_WEEEKENDS =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_WEEKLY_EVERY_TIME_ON_THIS_CHANNEL =
  PVR_TIMER_TYPE_IS_REPEATING |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
  PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH;
  //PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

const int MPTV_RECORD_MANUAL =
  PVR_TIMER_TYPE_IS_MANUAL |
  PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
  PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
  PVR_TIMER_TYPE_SUPPORTS_START_TIME |
  PVR_TIMER_TYPE_SUPPORTS_END_TIME |
  PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
  //PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
  PVR_TIMER_TYPE_SUPPORTS_LIFETIME;

class cLifeTimeValues
{
  public:
    cLifeTimeValues();
    void SetLifeTimeValues(PVR_TIMER_TYPE& timertype);
  private:
    std::vector<std::pair<int, std::string>> m_lifetimeValues;
};

class cTimer
{
  public:
    cTimer();
    cTimer(const PVR_TIMER &timerinfo);
    virtual ~cTimer();

    void GetPVRtimerinfo(PVR_TIMER &tag);
    int Index(void) const { return m_index; }
    unsigned int Channel(void) const { return m_channel; }
    int Priority(void) { return Mepo2XBMCPriority(m_priority); }
    const char* Title(void) const { return m_title.c_str(); }
    const char* Dir(void) const { return m_directory.c_str(); }
    time_t StartTime(void) const;
    time_t EndTime(void) const;
    bool ParseLine(const char *s);
    int PreRecordInterval(void) const { return m_prerecordinterval; }
    int PostRecordInterval(void) const { return m_postrecordinterval; }
    int RepeatFlags() { return SchedRecType2RepeatFlags(m_schedtype); };
    bool Repeat() const { return (m_schedtype == TvDatabase::Once ? false : true); };
    bool Done() const { return m_done; };
    bool IsManual() const { return m_ismanual; };
    bool IsActive() const { return (m_canceled==MPTV::cUndefinedDate); };
    bool IsRecording() const { return m_isrecording; };
    TvDatabase::ScheduleRecordingType RepeatFlags2SchedRecType(int repeatflags);
    std::string AddScheduleCommand();
    std::string UpdateScheduleCommand();
    void SetScheduleRecordingType(TvDatabase::ScheduleRecordingType schedType);
    void SetKeepMethod(TvDatabase::KeepMethodType keepmethod);
    void SetPreRecordInterval(int minutes);
    void SetPostRecordInterval(int minutes);

    void SetGenreTable(CGenreTable* genretable);

  private:
    int SchedRecType2RepeatFlags(TvDatabase::ScheduleRecordingType schedtype);

    /**
     * @brief Convert a XBMC Lifetime value to MediaPortals keepMethod+keepDate settings
     * @param lifetime the XBMC lifetime value (in days) (following the VDR syntax)
     * Should be called after setting m_starttime !!
     */
    void SetKeepMethod(int lifetime);
    int GetLifetime(void);
    int XBMC2MepoPriority(int xbmcprio);
    int Mepo2XBMCPriority(int mepoprio);

    // MediaPortal database fields:
    int         m_index;               ///> MediaPortal id_Schedule
    int         m_channel;             ///> MediaPortal idChannel
    TvDatabase::ScheduleRecordingType m_schedtype; ///> MediaPortal scheduleType
    std::string m_title;               ///> MediaPortal programName
    MPTV::CDateTime m_startTime;       ///> MediaPortal startTime
    MPTV::CDateTime m_endTime;         ///> MediaPortal endTime
    //                                      skipped: maxAirings field = episodes to keep
    int         m_priority;            ///> MediaPortal priority (not the XBMC one!!!)
    std::string m_directory;           ///> MediaPortal directory
    //                                      skipped:  quality field
    TvDatabase::KeepMethodType m_keepmethod;       ///> MediaPortal keepMethod
    MPTV::CDateTime m_keepDate;        ///> MediaPortal keepDate
    int         m_prerecordinterval;   ///> MediaPortal preRecordInterval
    int         m_postrecordinterval;  ///> MediaPortal postRecordInterval
    MPTV::CDateTime m_canceled;        ///> MediaPortal canceled (date + time)
    //                                      skipped: recommendedCard
    bool        m_series;              ///> MediaPortal series
    int         m_parentScheduleID;    ///> MediaPortal idParentSchedule

    // XBMC asks for these fields:
    bool        m_active;
    bool        m_done;
    bool        m_ismanual;
    bool        m_isrecording;

    int         m_progid;              ///> MediaPortal Program ID
    std::string m_genre;               ///> The genre string for the program
    std::string m_description;         ///> Program description

    CGenreTable* m_genretable;
};

namespace Timer
{
  // Life time values for the recordings
  extern cLifeTimeValues* lifetimeValues;
};

#endif //__TIMERS_H
