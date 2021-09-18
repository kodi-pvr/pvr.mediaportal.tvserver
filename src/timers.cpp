/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <vector>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#include "timers.h"
#include "settings.h"
#include "utils.h"
#include "DateTime.h"
#include "epg.h"

#include <kodi/General.h>

using namespace MPTV;

cTimer::cTimer() :
  m_keepDate(cUndefinedDate),
  m_canceled(cUndefinedDate)
{
  m_index              = PVR_TIMER_NO_CLIENT_INDEX;
  m_active             = true;
  m_channel            = PVR_CHANNEL_INVALID_UID;
  m_schedtype          = TvDatabase::Once;
  m_priority           = 0;
  m_keepmethod         = TvDatabase::UntilSpaceNeeded;
  m_prerecordinterval  = -1; // Use MediaPortal setting instead
  m_postrecordinterval = -1; // Use MediaPortal setting instead
  m_series             = false;
  m_done               = false;
  m_ismanual           = false;
  m_isrecording        = false;
  m_progid             = (EPG_TAG_INVALID_UID - cKodiEpgIndexOffset);
  m_genretable         = NULL;
  m_parentScheduleID   = MPTV_NO_PARENT_SCHEDULE;
}


cTimer::cTimer(const kodi::addon::PVRTimer& timerinfo):
  m_genretable(NULL)
{

  m_index = timerinfo.GetClientIndex() - cKodiTimerIndexOffset;
  m_progid = timerinfo.GetEPGUid() - cKodiEpgIndexOffset;

  m_parentScheduleID = timerinfo.GetParentClientIndex() - cKodiTimerIndexOffset;

  if (m_index >= MPTV_REPEAT_NO_SERIES_OFFSET)
  {
    m_index = m_parentScheduleID;
  }

  m_done = (timerinfo.GetState() == PVR_TIMER_STATE_COMPLETED);
  m_active = (timerinfo.GetState() == PVR_TIMER_STATE_SCHEDULED || timerinfo.GetState() == PVR_TIMER_STATE_RECORDING
    || timerinfo.GetState() == PVR_TIMER_STATE_CONFLICT_OK || timerinfo.GetState() == PVR_TIMER_STATE_CONFLICT_NOK);

  if (!m_active)
  {
    // Don't know when it was cancelled, so assume that it was canceled now...
    // backend (TVServerKodi) will only update the canceled date time when
    // this schedule was just canceled
    m_canceled = CDateTime::Now();
  }
  else
  {
    m_canceled = cUndefinedDate;
  }

  m_title = timerinfo.GetTitle();
  m_directory = timerinfo.GetDirectory();
  m_channel = timerinfo.GetClientChannelUid();

  if (timerinfo.GetStartTime() <= 0)
  {
    // Instant timer has starttime = 0, so set current time as starttime.
    m_startTime = CDateTime::Now();
    m_ismanual = true;
  }
  else
  {
    m_startTime = timerinfo.GetStartTime();
    m_ismanual = false;
  }

  m_endTime = timerinfo.GetEndTime();
  //m_firstday = timerinfo.firstday;
  m_isrecording = (timerinfo.GetState() == PVR_TIMER_STATE_RECORDING);
  m_priority = XBMC2MepoPriority(timerinfo.GetPriority());

  SetKeepMethod(timerinfo.GetLifetime());

  m_schedtype = static_cast<enum TvDatabase::ScheduleRecordingType>(timerinfo.GetTimerType() - cKodiTimerTypeOffset);
  if (m_schedtype == TvDatabase::KodiManual)
  {
    m_schedtype = TvDatabase::Once;
  }

  if ((m_schedtype == TvDatabase::Once) && (timerinfo.GetWeekdays() != PVR_WEEKDAY_NONE)) // huh, still repeating?
  {
    // Select the correct schedule type
    m_schedtype = RepeatFlags2SchedRecType(timerinfo.GetWeekdays());
  }

  m_series = (m_schedtype == TvDatabase::Once) ? false : true;

  m_prerecordinterval = timerinfo.GetMarginStart();
  m_postrecordinterval = timerinfo.GetMarginEnd();
}


cTimer::~cTimer()
{
}

/**
 * @brief Fills the PVR_TIMER struct with information from this timer
 * @param tag A reference to the PVR_TIMER struct
 */
void cTimer::GetPVRtimerinfo(kodi::addon::PVRTimer &tag)
{
  if (m_parentScheduleID != MPTV_NO_PARENT_SCHEDULE)
  {
    /* Hack: use a different client index for Kodi since it does not accept multiple times the same schedule id
     * This means that all programs scheduled via a series schedule in MediaPortal will get a different client
     * index in Kodi. The iParentClientIndex will still point to the original id_Schedule in MediaPortal
     */
    tag.SetClientIndex(cKodiTimerIndexOffset + MPTV_REPEAT_NO_SERIES_OFFSET + cKodiEpgIndexOffset + m_progid);
  }
  else
  {
    tag.SetClientIndex(cKodiTimerIndexOffset + m_index);
  }
  tag.SetEPGUid(cKodiEpgIndexOffset + m_progid);

  if (IsRecording())
    tag.SetState(PVR_TIMER_STATE_RECORDING);
  else if (m_active)
    tag.SetState(PVR_TIMER_STATE_SCHEDULED);
  else
    tag.SetState(PVR_TIMER_STATE_DISABLED);

  if (m_schedtype == TvDatabase::EveryTimeOnEveryChannel)
  {
    tag.SetClientChannelUid(PVR_TIMER_ANY_CHANNEL);
  }
  else
  {
    tag.SetClientChannelUid(m_channel);
  }
  tag.SetTitle(m_title);
  tag.SetStartTime(m_startTime.GetAsTime());
  tag.SetEndTime(m_endTime.GetAsTime());
  // From the VDR manual
  // firstday: The date of the first day when this timer shall start recording
  //           (only available for repeating timers).
  if(Repeat())
  {
    if (m_parentScheduleID != MPTV_NO_PARENT_SCHEDULE)
    {
      tag.SetFirstDay(0);
      tag.SetParentClientIndex((unsigned int)(cKodiTimerIndexOffset + m_parentScheduleID));
      tag.SetWeekdays(PVR_WEEKDAY_NONE);
      tag.SetTimerType(cKodiTimerTypeOffset + (int) TvDatabase::Once);
      tag.SetClientChannelUid(m_channel);
    }
    else
    {
      tag.SetFirstDay(m_startTime.GetAsTime());
      tag.SetParentClientIndex(PVR_TIMER_NO_PARENT);
      tag.SetWeekdays(RepeatFlags());
      tag.SetTimerType(cKodiTimerTypeOffset + (int) m_schedtype);
    }
  }
  else
  {
    tag.SetFirstDay(0);
    tag.SetParentClientIndex(PVR_TIMER_NO_PARENT);
    tag.SetWeekdays(RepeatFlags());
    tag.SetTimerType(cKodiTimerTypeOffset + (int) m_schedtype);
  }
  tag.SetPriority(Priority());
  tag.SetLifetime(GetLifetime());
  tag.SetMarginStart(m_prerecordinterval);
  tag.SetMarginEnd(m_postrecordinterval);
  if (m_genretable)
  {
    // genre string to genre type/subtype mapping
    int genreType;
    int genreSubType;
    m_genretable->GenreToTypes(m_genre, genreType, genreSubType);
    tag.SetGenreType(genreType);
    tag.SetGenreSubType(genreSubType);
  }
  else
  {
    tag.SetGenreType(0);
    tag.SetGenreSubType(0);
  }
  tag.SetDirectory(m_directory);
  tag.SetSummary(m_description);
}

time_t cTimer::StartTime(void) const
{
  time_t retVal = m_startTime.GetAsTime();
  return retVal;
}

time_t cTimer::EndTime(void) const
{
  time_t retVal = m_endTime.GetAsTime();
  return retVal;
}

bool cTimer::ParseLine(const char *s)
{
  vector<string> schedulefields;
  string data = s;
  uri::decode(data);

  Tokenize(data, schedulefields, "|");

  if (schedulefields.size() >= 10)
  {
    // field 0 = index
    // field 1 = start date + time
    // field 2 = end   date + time
    // field 3 = channel nr
    // field 4 = channel name
    // field 5 = program name
    // field 6 = schedule recording type
    // field 7 = priority
    // field 8 = isdone (finished)
    // field 9 = ismanual
    // field 10 = directory
    // field 11 = keepmethod (0=until space needed, 1=until watched, 2=until keepdate, 3=forever) (TVServerKodi build >= 100)
    // field 12 = keepdate (2000-01-01 00:00:00 = infinite)  (TVServerKodi build >= 100)
    // field 13 = preRecordInterval  (TVServerKodi build >= 100)
    // field 14 = postRecordInterval (TVServerKodi build >= 100)
    // field 15 = canceled (TVServerKodi build >= 100)
    // field 16 = series (True/False) (TVServerKodi build >= 100)
    // field 17 = isrecording (True/False)
    // field 18 = program id (EPG)
    // field 19 = parent schedule id (TVServerKodi build >= 130)
    // field 20 = genre of the program (TVServerKodi build >= 130)
    // field 21 = program description (EPG) (TVServerKodi build >= 130)
    m_index = atoi(schedulefields[0].c_str());

    if ( m_startTime.SetFromDateTime(schedulefields[1]) == false )
      return false;

    if ( m_endTime.SetFromDateTime(schedulefields[2]) == false )
      return false;

    m_channel = atoi(schedulefields[3].c_str());
    m_title = schedulefields[5];

    m_schedtype = (TvDatabase::ScheduleRecordingType) atoi(schedulefields[6].c_str());

    m_priority = atoi(schedulefields[7].c_str());
    m_done = stringtobool(schedulefields[8]);
    m_ismanual = stringtobool(schedulefields[9]);
    m_directory = schedulefields[10];

    if(schedulefields.size() >= 18)
    {
      //TVServerKodi build >= 100
      m_keepmethod = (TvDatabase::KeepMethodType) atoi(schedulefields[11].c_str());
      if ( m_keepDate.SetFromDateTime(schedulefields[12]) == false )
        return false;

      m_prerecordinterval = atoi(schedulefields[13].c_str());
      m_postrecordinterval = atoi(schedulefields[14].c_str());

      // The DateTime value 2000-01-01 00:00:00 means: active in MediaPortal
      if(schedulefields[15].compare("2000-01-01 00:00:00Z")==0)
      {
        m_canceled.SetFromTime(MPTV::cUndefinedDate);
        m_active = true;
      }
      else
      {
        if (m_canceled.SetFromDateTime(schedulefields[15]) == false)
        {
          m_canceled.SetFromTime(MPTV::cUndefinedDate);
        }
        m_active = false;
      }

      m_series = stringtobool(schedulefields[16]);
      m_isrecording = stringtobool(schedulefields[17]);

    }
    else
    {
      m_keepmethod = TvDatabase::UntilSpaceNeeded;
      m_keepDate = cUndefinedDate;
      m_prerecordinterval = -1;
      m_postrecordinterval = -1;
      m_canceled = cUndefinedDate;
      m_active = true;
      m_series = false;
      m_isrecording = false;
    }

    if(schedulefields.size() >= 19)
      m_progid = atoi(schedulefields[18].c_str());
    else
      m_progid = (EPG_TAG_INVALID_UID - cKodiEpgIndexOffset);

    if (schedulefields.size() >= 22)
    {
      m_parentScheduleID = atoi(schedulefields[19].c_str());
      m_genre = schedulefields[20];
      m_description = schedulefields[21];
    }
    else
    {
      m_parentScheduleID = MPTV_NO_PARENT_SCHEDULE;
      m_genre.clear();
      m_description.clear();
    }

    return true;
  }
  return false;
}

int cTimer::SchedRecType2RepeatFlags(TvDatabase::ScheduleRecordingType schedtype)
{
  //   This field contains a bitmask that corresponds to the days of the week at which this timer runs
  //   It is based on the VDR Day field format "MTWTF--"
  //   The format is a 1 bit for every enabled day and a 0 bit for a disabled day
  //   Thus: WeekDays = "0000 0001" = "M------" (monday only)
  //                    "0110 0000" = "-----SS" (saturday and sunday)
  //                    "0001 1111" = "MTWTF--" (all weekdays)

  int weekdays = 0;

  switch (schedtype)
  {
    case TvDatabase::Once:
    case TvDatabase::KodiManual:
      weekdays = PVR_WEEKDAY_NONE;
      break;
    case TvDatabase::Daily:
      weekdays = PVR_WEEKDAY_ALLDAYS; // 0111 1111
      break;
    case TvDatabase::Weekly:
    case TvDatabase::WeeklyEveryTimeOnThisChannel:
      {
        // Not sure what to do with these MediaPortal options...
        // Assumption: record once a week, on the same day and time
        // => determine weekday and set the corresponding bit
        int weekday = m_startTime.GetDayOfWeek(); //days since Sunday [0-6]
        // bit 0 = monday, need to convert weekday value to bitnumber:
        if (weekday == 0)
          weekday = 6; // sunday 0100 0000
        else
          weekday--;

        weekdays = 1 << weekday;
        break;
      }
    case TvDatabase::EveryTimeOnThisChannel:
      // Don't know what to do with this MediaPortal option?
      weekdays = PVR_WEEKDAY_ALLDAYS; // 0111 1111 (daily)
      break;
    case TvDatabase::EveryTimeOnEveryChannel:
      // Don't know what to do with this MediaPortal option?
      weekdays = PVR_WEEKDAY_ALLDAYS; // 0111 1111 (daily)
      break;
    case TvDatabase::Weekends:
      // 0110 0000
      weekdays = PVR_WEEKDAY_SATURDAY | PVR_WEEKDAY_SUNDAY;
      break;
    case TvDatabase::WorkingDays:
      // 0001 1111
      weekdays = PVR_WEEKDAY_MONDAY | PVR_WEEKDAY_TUESDAY | PVR_WEEKDAY_WEDNESDAY | PVR_WEEKDAY_THURSDAY | PVR_WEEKDAY_FRIDAY;
      break;
    default:
      weekdays = PVR_WEEKDAY_NONE;
  }

  return weekdays;
}

TvDatabase::ScheduleRecordingType cTimer::RepeatFlags2SchedRecType(int repeatflags)
{
  // margro: the meaning of the XBMC-PVR Weekdays field is undocumented.
  // Assuming that VDR is the source for this field:
  //   This field contains a bitmask that corresponds to the days of the week at which this timer runs
  //   It is based on the VDR Day field format "MTWTF--"
  //   The format is a 1 bit for every enabled day and a 0 bit for a disabled day
  //   Thus: WeekDays = "0000 0001" = "M------" (monday only)
  //                    "0110 0000" = "-----SS" (saturday and sunday)
  //                    "0001 1111" = "MTWTF--" (all weekdays)

  switch (repeatflags)
  {
    case PVR_WEEKDAY_NONE:
      return TvDatabase::Once;
      break;
    case PVR_WEEKDAY_MONDAY:
    case PVR_WEEKDAY_TUESDAY:
    case PVR_WEEKDAY_WEDNESDAY:
    case PVR_WEEKDAY_THURSDAY:
    case PVR_WEEKDAY_FRIDAY:
    case PVR_WEEKDAY_SATURDAY:
    case PVR_WEEKDAY_SUNDAY:
      return TvDatabase::Weekly;
      break;
    case (PVR_WEEKDAY_MONDAY | PVR_WEEKDAY_TUESDAY | PVR_WEEKDAY_WEDNESDAY | PVR_WEEKDAY_THURSDAY | PVR_WEEKDAY_FRIDAY):  // 0001 1111
      return TvDatabase::WorkingDays;
    case (PVR_WEEKDAY_SATURDAY | PVR_WEEKDAY_SUNDAY):  // 0110 0000
      return TvDatabase::Weekends;
      break;
    case PVR_WEEKDAY_ALLDAYS: // 0111 1111
      return TvDatabase::Daily;
      break;
    default:
      break;
  }

  return TvDatabase::Once;
}

std::string cTimer::AddScheduleCommand()
{
  char      command[1024];
  string startTime;
  string endTime;

  m_startTime.GetAsLocalizedTime(startTime);
  m_endTime.GetAsLocalizedTime(endTime);
  kodi::Log(ADDON_LOG_DEBUG, "Start time: %s, marginstart: %i min earlier", startTime.c_str(), m_prerecordinterval);
  kodi::Log(ADDON_LOG_DEBUG, "End time: %s, marginstop: %i min later", endTime.c_str(), m_postrecordinterval);

  snprintf(command, 1023, "AddSchedule:%i|%s|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i\n",
          m_channel,                                                         //Channel number [0]
          uri::encode(uri::PATH_TRAITS, m_title).c_str(),                    //Program title  [1]
          m_startTime.GetYear(), m_startTime.GetMonth(), m_startTime.GetDay(), //Start date     [2..4]
          m_startTime.GetHour(), m_startTime.GetMinute(), m_startTime.GetSecond(),             //Start time     [5..7]
          m_endTime.GetYear(), m_endTime.GetMonth(), m_endTime.GetDay(),       //End date       [8..10]
          m_endTime.GetHour(), m_endTime.GetMinute(), m_endTime.GetSecond(),                   //End time       [11..13]
          (int) m_schedtype, m_priority, (int) m_keepmethod,                 //SchedType, Priority, keepMethod [14..16]
          m_keepDate.GetYear(), m_keepDate.GetMonth(), m_keepDate.GetDay(),    //Keepdate       [17..19]
          m_keepDate.GetHour(), m_keepDate.GetMinute(), m_keepDate.GetSecond(),                //Keeptime       [20..22]
          m_prerecordinterval, m_postrecordinterval);                        //Prerecord,postrecord [23,24]

  return command;
}

std::string cTimer::UpdateScheduleCommand()
{
  char      command[1024];
  string startTime;
  string endTime;

  m_startTime.GetAsLocalizedTime(startTime);
  m_endTime.GetAsLocalizedTime(endTime);
  kodi::Log(ADDON_LOG_DEBUG, "Start time: %s, marginstart: %i min earlier", startTime.c_str(), m_prerecordinterval);
  kodi::Log(ADDON_LOG_DEBUG, "End time: %s, marginstop: %i min later", endTime.c_str(), m_postrecordinterval);

  snprintf(command, 1024, "UpdateSchedule:%i|%i|%i|%s|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i|%i\n",
          m_index,                                                           //Schedule index [0]
          m_active,                                                          //Active         [1]
          m_channel,                                                         //Channel number [2]
          uri::encode(uri::PATH_TRAITS,m_title).c_str(),                     //Program title  [3]
          m_startTime.GetYear(), m_startTime.GetMonth(), m_startTime.GetDay(), //Start date     [4..6]
          m_startTime.GetHour(), m_startTime.GetMinute(), m_startTime.GetSecond(),             //Start time     [7..9]
          m_endTime.GetYear(), m_endTime.GetMonth(), m_endTime.GetDay(),       //End date       [10..12]
          m_endTime.GetHour(), m_endTime.GetMinute(), m_endTime.GetSecond(),                   //End time       [13..15]
          (int) m_schedtype, m_priority, (int) m_keepmethod,                 //SchedType, Priority, keepMethod [16..18]
          m_keepDate.GetYear(), m_keepDate.GetMonth(), m_keepDate.GetDay(),    //Keepdate       [19..21]
          m_keepDate.GetHour(), m_keepDate.GetMinute(), m_keepDate.GetSecond(),                //Keeptime       [22..24]
          m_prerecordinterval, m_postrecordinterval, m_progid);              //Prerecord,postrecord,program_id [25,26,27]

  return command;
}


int cTimer::XBMC2MepoPriority(int UNUSED(xbmcprio))
{
  // From XBMC side: 0.99 where 0=lowest and 99=highest priority (like VDR). Default value: 50
  // Meaning of the MediaPortal field is unknown to me. Default seems to be 0.
  // TODO: figure out the mapping
  return 0;
}

int cTimer::Mepo2XBMCPriority(int UNUSED(mepoprio))
{
  return 50; //Default value
}


/*
 * @brief Convert a Kodi Lifetime value to MediaPortals keepMethod+keepDate settings
 * @param lifetime the Kodi lifetime value (in days)
 * Should be called after setting m_starttime !!
 */
void cTimer::SetKeepMethod(int lifetime)
{
  // Kodi keep methods:
  // negative values: => special methods like Until Space Needed, Always, Until Watched
  // positive values: days to keep the recording
  if (lifetime == 0)
  {
    m_keepmethod = TvDatabase::UntilSpaceNeeded;
    m_keepDate = cUndefinedDate;
  }
  else if (lifetime < 0)
  {
    m_keepmethod = (TvDatabase::KeepMethodType) -lifetime;
    m_keepDate = cUndefinedDate;
  }
  else
  {
    m_keepmethod = TvDatabase::TillDate;
    m_keepDate = m_startTime;
    m_keepDate += (lifetime * cSecsInDay);
  }
}

int cTimer::GetLifetime(void)
{
  // lifetime of recordings created by this timer.
  // value > 0 = days after which recordings will be deleted by the backend,
  // value < 0 addon defined integer list reference,
  // value == 0 disabled
  switch (m_keepmethod)
  {
    case TvDatabase::UntilSpaceNeeded: //until space needed
      return -MPTV_KEEP_UNTIL_SPACE_NEEDED;
      break;
    case TvDatabase::UntilWatched: //until watched
      return -MPTV_KEEP_UNTIL_WATCHED;
      break;
    case TvDatabase::TillDate: //until keepdate
      {
        int diffseconds = m_keepDate - m_startTime;
        int daysremaining = (int)(diffseconds / cSecsInDay);
        // Calculate value in the range 1...98, based on m_keepdate
        return daysremaining;
      }
      break;
    case TvDatabase::Always: //forever
      return -MPTV_KEEP_ALWAYS;
    default:
      return 0;
  }
}

void cTimer::SetScheduleRecordingType(TvDatabase::ScheduleRecordingType schedType)
{
  m_schedtype = schedType;
}

void cTimer::SetKeepMethod(TvDatabase::KeepMethodType keepmethod)
{
  m_keepmethod = keepmethod;
}

void cTimer::SetPreRecordInterval(int minutes)
{
  m_prerecordinterval = minutes;
}

void cTimer::SetPostRecordInterval(int minutes)
{
  m_postrecordinterval = minutes;
}

void cTimer::SetGenreTable(CGenreTable* genretable)
{
  m_genretable = genretable;
}

cLifeTimeValues::cLifeTimeValues()
{
  /* Prepare the list with Lifetime values and descriptions */
  // MediaPortal keep methods:
  m_lifetimeValues.emplace_back(-MPTV_KEEP_ALWAYS, kodi::GetLocalizedString(30133));
  m_lifetimeValues.emplace_back(-MPTV_KEEP_UNTIL_SPACE_NEEDED, kodi::GetLocalizedString(30130));
  m_lifetimeValues.emplace_back(-MPTV_KEEP_UNTIL_WATCHED, kodi::GetLocalizedString(30131));

  //Not directly supported by Kodi. I can add this, but there is no way to select the date
  //m_lifetimeValues.emplace_back(TvDatabase::TillDate, kodi::GetLocalizedString(30132));

  // MediaPortal Until date replacements:
  const std::string strWeeks = kodi::GetLocalizedString(30137); // %d weeks
  const std::string strMonths = kodi::GetLocalizedString(30139); // %d months
  const size_t cKeepStringLength = 255;
  char strKeepString[cKeepStringLength];

  m_lifetimeValues.emplace_back(MPTV_KEEP_ONE_WEEK, kodi::GetLocalizedString(30134));

  snprintf(strKeepString, cKeepStringLength, strWeeks.c_str(), 2);
  m_lifetimeValues.emplace_back(MPTV_KEEP_TWO_WEEKS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strWeeks.c_str(), 3);
  m_lifetimeValues.emplace_back(MPTV_KEEP_THREE_WEEKS, strKeepString);

  m_lifetimeValues.emplace_back(MPTV_KEEP_ONE_MONTH, kodi::GetLocalizedString(30138));

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 2);
  m_lifetimeValues.emplace_back(MPTV_KEEP_TWO_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 3);
  m_lifetimeValues.emplace_back(MPTV_KEEP_THREE_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 4);
  m_lifetimeValues.emplace_back(MPTV_KEEP_FOUR_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 5);
  m_lifetimeValues.emplace_back(MPTV_KEEP_FIVE_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 6);
  m_lifetimeValues.emplace_back(MPTV_KEEP_SIX_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 7);
  m_lifetimeValues.emplace_back(MPTV_KEEP_SEVEN_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 8);
  m_lifetimeValues.emplace_back(MPTV_KEEP_EIGHT_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 9);
  m_lifetimeValues.emplace_back(MPTV_KEEP_NINE_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 10);
  m_lifetimeValues.emplace_back(MPTV_KEEP_TEN_MONTHS, strKeepString);

  snprintf(strKeepString, cKeepStringLength, strMonths.c_str(), 11);
  m_lifetimeValues.emplace_back(MPTV_KEEP_ELEVEN_MONTHS, strKeepString);

  m_lifetimeValues.emplace_back(MPTV_KEEP_ONE_YEAR, kodi::GetLocalizedString(30140));
}

void cLifeTimeValues::SetLifeTimeValues(kodi::addon::PVRTimerType& timertype)
{
  timertype.SetLifetimes(m_lifetimeValues, -MPTV_KEEP_ALWAYS); //Negative = special types, positive values is days

  //select default keep method
  switch (CSettings::Get().GetKeepMethodType())
  {
    case TvDatabase::UntilSpaceNeeded: //until space needed
      timertype.SetLifetimesDefault(-MPTV_KEEP_UNTIL_SPACE_NEEDED); //Negative = special types, positive values is days
      break;

    case TvDatabase::UntilWatched: //until watched
      timertype.SetLifetimesDefault(-MPTV_KEEP_UNTIL_WATCHED); //Negative = special types, positive values is days
      break;

    case TvDatabase::TillDate: //until keepdate
      //use defaultrecordinglifetime value from settings.xml
      timertype.SetLifetimesDefault(CSettings::Get().GetDefaultRecordingLifeTime());
      break;

    case TvDatabase::Always: //forever
    default:
      break;
  }
}

namespace Timer
{
  // Life time values for the recordings
  cLifeTimeValues* lifetimeValues = NULL;
};
