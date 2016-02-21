/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <vector>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#include "p8-platform/os.h" //needed for snprintf
#include "client.h"
#include "timers.h"
#include "utils.h"
#include "DateTime.h"
#include "epg.h"

using namespace ADDON;
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


cTimer::cTimer(const PVR_TIMER& timerinfo):
  m_genretable(NULL)
{

  m_index = timerinfo.iClientIndex - cKodiTimerIndexOffset;
  m_progid = timerinfo.iEpgUid - cKodiEpgIndexOffset;

  m_parentScheduleID = timerinfo.iParentClientIndex - cKodiTimerIndexOffset;

  if (m_index >= MPTV_REPEAT_NO_SERIES_OFFSET)
  {
    m_index = m_parentScheduleID;
  }

  m_done = (timerinfo.state == PVR_TIMER_STATE_COMPLETED);
  m_active = (timerinfo.state == PVR_TIMER_STATE_SCHEDULED || timerinfo.state == PVR_TIMER_STATE_RECORDING
    || timerinfo.state == PVR_TIMER_STATE_CONFLICT_OK || timerinfo.state == PVR_TIMER_STATE_CONFLICT_NOK);

  if (!m_active)
  {
    // Don't know when it was cancelled, so assume that it was canceled now...
    // backend (TVServerXBMC) will only update the canceled date time when
    // this schedule was just canceled
    m_canceled = CDateTime::Now();
  }
  else
  {
    m_canceled = cUndefinedDate;
  }
 
  m_title = timerinfo.strTitle;
  m_directory = timerinfo.strDirectory;
  m_channel = timerinfo.iClientChannelUid;

  if (timerinfo.startTime <= 0)
  {
    // Instant timer has starttime = 0, so set current time as starttime.
    m_startTime = CDateTime::Now();
    m_ismanual = true;
  }
  else
  {
    m_startTime = timerinfo.startTime;
    m_ismanual = false;
  }

  m_endTime = timerinfo.endTime;
  //m_firstday = timerinfo.firstday;
  m_isrecording = (timerinfo.state == PVR_TIMER_STATE_RECORDING);
  m_priority = XBMC2MepoPriority(timerinfo.iPriority);

  SetKeepMethod(timerinfo.iLifetime);

  m_schedtype = static_cast<enum TvDatabase::ScheduleRecordingType>(timerinfo.iTimerType - cKodiTimerTypeOffset);
  if (m_schedtype == TvDatabase::KodiManual)
  {
    m_schedtype = TvDatabase::Once;
  }

  if ((m_schedtype == TvDatabase::Once) && (timerinfo.iWeekdays != PVR_WEEKDAY_NONE)) // huh, still repeating?
  {
    // Select the correct schedule type
    m_schedtype = RepeatFlags2SchedRecType(timerinfo.iWeekdays);
  }

  m_series = (m_schedtype == TvDatabase::Once) ? false : true;

  m_prerecordinterval = timerinfo.iMarginStart;
  m_postrecordinterval = timerinfo.iMarginEnd;
}


cTimer::~cTimer()
{
}

/**
 * @brief Fills the PVR_TIMER struct with information from this timer
 * @param tag A reference to the PVR_TIMER struct
 */
void cTimer::GetPVRtimerinfo(PVR_TIMER &tag)
{
  memset(&tag, 0, sizeof(tag));

  if (m_parentScheduleID != MPTV_NO_PARENT_SCHEDULE)
  {
    /* Hack: use a different client index for Kodi since it does not accept multiple times the same schedule id
     * This means that all programs scheduled via a series schedule in MediaPortal will get a different client
     * index in Kodi. The iParentClientIndex will still point to the original id_Schedule in MediaPortal
     */
    tag.iClientIndex = cKodiTimerIndexOffset + MPTV_REPEAT_NO_SERIES_OFFSET + cKodiEpgIndexOffset + m_progid;
  }
  else
  {
    tag.iClientIndex = cKodiTimerIndexOffset + m_index;
  }
  tag.iEpgUid = cKodiEpgIndexOffset + m_progid;

  if (IsRecording())
    tag.state           = PVR_TIMER_STATE_RECORDING;
  else if (m_active)
    tag.state           = PVR_TIMER_STATE_SCHEDULED;
  else
    tag.state           = PVR_TIMER_STATE_DISABLED;

  if (m_schedtype == TvDatabase::EveryTimeOnEveryChannel)
  {
    tag.iClientChannelUid = PVR_TIMER_ANY_CHANNEL;
  }
  else
  {
    tag.iClientChannelUid = m_channel;
  }
  PVR_STRCPY(tag.strTitle, m_title.c_str());
  tag.startTime = m_startTime.GetAsTime();
  tag.endTime = m_endTime.GetAsTime();
  // From the VDR manual
  // firstday: The date of the first day when this timer shall start recording
  //           (only available for repeating timers).
  if(Repeat())
  {
    if (m_parentScheduleID != MPTV_NO_PARENT_SCHEDULE)
    {
      tag.firstDay = 0;
      tag.iParentClientIndex = (unsigned int)(cKodiTimerIndexOffset + m_parentScheduleID);
      tag.iWeekdays = PVR_WEEKDAY_NONE;
      tag.iTimerType = cKodiTimerTypeOffset + (int) TvDatabase::Once;
      tag.iClientChannelUid = m_channel;
    }
    else
    {
      tag.firstDay = m_startTime.GetAsTime();
      tag.iParentClientIndex = PVR_TIMER_NO_PARENT;
      tag.iWeekdays = RepeatFlags();
      tag.iTimerType = cKodiTimerTypeOffset + (int) m_schedtype;
    }
  }
  else
  {
    tag.firstDay = 0;
    tag.iParentClientIndex = PVR_TIMER_NO_PARENT;
    tag.iWeekdays = RepeatFlags();
    tag.iTimerType = cKodiTimerTypeOffset + (int) m_schedtype;
  }
  tag.iPriority = Priority();
  tag.iLifetime = GetLifetime();
  tag.iMarginStart = m_prerecordinterval;
  tag.iMarginEnd = m_postrecordinterval;
  if (m_genretable)
  {
    // genre string to genre type/subtype mapping
    int genreType;
    int genreSubType;
    m_genretable->GenreToTypes(m_genre, genreType, genreSubType);
    tag.iGenreType = genreType;
    tag.iGenreSubType = genreSubType;
  }
  else
  {
    tag.iGenreType = 0;
    tag.iGenreSubType = 0;
  }
  PVR_STRCPY(tag.strDirectory, m_directory.c_str());
  PVR_STRCPY(tag.strSummary, m_description.c_str());
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
    // field 11 = keepmethod (0=until space needed, 1=until watched, 2=until keepdate, 3=forever) (TVServerXBMC build >= 100)
    // field 12 = keepdate (2000-01-01 00:00:00 = infinite)  (TVServerXBMC build >= 100)
    // field 13 = preRecordInterval  (TVServerXBMC build >= 100)
    // field 14 = postRecordInterval (TVServerXBMC build >= 100)
    // field 15 = canceled (TVServerXBMC build >= 100)
    // field 16 = series (True/False) (TVServerXBMC build >= 100)
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
      //TVServerXBMC build >= 100
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
  XBMC->Log(LOG_DEBUG, "Start time: %s, marginstart: %i min earlier", startTime.c_str(), m_prerecordinterval);
  XBMC->Log(LOG_DEBUG, "End time: %s, marginstop: %i min later", endTime.c_str(), m_postrecordinterval);

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
  XBMC->Log(LOG_DEBUG, "Start time: %s, marginstart: %i min earlier", startTime.c_str(), m_prerecordinterval);
  XBMC->Log(LOG_DEBUG, "End time: %s, marginstop: %i min later", endTime.c_str(), m_postrecordinterval);

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
  m_lifetimeValues.push_back(std::make_pair(-MPTV_KEEP_ALWAYS, XBMC->GetLocalizedString(30133)));
  m_lifetimeValues.push_back(std::make_pair(-MPTV_KEEP_UNTIL_SPACE_NEEDED, XBMC->GetLocalizedString(30130)));
  m_lifetimeValues.push_back(std::make_pair(-MPTV_KEEP_UNTIL_WATCHED, XBMC->GetLocalizedString(30131)));

  //Not directly supported by Kodi. I can add this, but there is no way to select the date
  //m_lifetimeValues.push_back(std::make_pair(TvDatabase::TillDate, XBMC->GetLocalizedString(30132)));

  // MediaPortal Until date replacements:
  const char* strWeeks = XBMC->GetLocalizedString(30137); // %d weeks
  const char* strMonths = XBMC->GetLocalizedString(30139); // %d months
  const size_t cKeepStringLength = 255;
  char strKeepString[cKeepStringLength];

  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_ONE_WEEK, XBMC->GetLocalizedString(30134)));

  snprintf(strKeepString, cKeepStringLength, strWeeks, 2);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_TWO_WEEKS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strWeeks, 3);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_THREE_WEEKS, strKeepString));

  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_ONE_MONTH, XBMC->GetLocalizedString(30138)));

  snprintf(strKeepString, cKeepStringLength, strMonths, 2);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_TWO_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 3);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_THREE_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 4);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_FOUR_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 5);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_FIVE_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 6);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_SIX_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 7);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_SEVEN_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 8);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_EIGHT_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 9);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_NINE_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 10);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_TEN_MONTHS, strKeepString));

  snprintf(strKeepString, cKeepStringLength, strMonths, 11);
  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_ELEVEN_MONTHS, strKeepString));

  m_lifetimeValues.push_back(std::make_pair(MPTV_KEEP_ONE_YEAR, XBMC->GetLocalizedString(30140)));
}

void cLifeTimeValues::SetLifeTimeValues(PVR_TIMER_TYPE& timertype)
{
  timertype.iLifetimesSize = m_lifetimeValues.size();
  timertype.iLifetimesDefault = -MPTV_KEEP_ALWAYS; //Negative = special types, positive values is days

  int i = 0;
  std::vector<std::pair<int, std::string>>::iterator it;
  for (it = m_lifetimeValues.begin(); ((it != m_lifetimeValues.end()) && (i < PVR_ADDON_TIMERTYPE_VALUES_ARRAY_SIZE)); ++it, ++i)
  {
    timertype.lifetimes[i].iValue = it->first;
    PVR_STRCPY(timertype.lifetimes[i].strDescription, it->second.c_str());
  }
}

namespace Timer
{
  // Life time values for the recordings
  cLifeTimeValues* lifetimeValues = NULL;
};
