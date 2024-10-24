/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <clocale>

#include <kodi/tools/StringUtils.h>

#include "timers.h"
#include "channels.h"
#include "recordings.h"
#include "epg.h"
#include "settings.h"
#include "utils.h"
#include "pvrclient-mediaportal.h"
#include "lib/tsreader/TSReader.h"
#ifdef TARGET_WINDOWS
#include "FileUtils.h"
#endif
#include "GUIDialogRecordSettings.h"

#include <kodi/General.h>
#include <kodi/Filesystem.h>

using namespace kodi::tools;
using namespace std;
using namespace MPTV;

/* Globals */
int g_iTVServerKodiBuild = 0;

/* TVServerKodi plugin supported versions */
#define TVSERVERKODI_MIN_VERSION_STRING         "1.1.7.107"
#define TVSERVERKODI_MIN_VERSION_BUILD          107
#define TVSERVERKODI_RECOMMENDED_VERSION_STRING "1.2.3.122 till 1.20.0.140"
#define TVSERVERKODI_RECOMMENDED_VERSION_BUILD  140

template<typename T> void SafeDelete(T*& p)
{
  if (p)
  {
    delete p;
    p = nullptr;
  }
}

/************************************************************/
/** Class interface */

cPVRClientMediaPortal::cPVRClientMediaPortal(const kodi::addon::IInstanceInfo& instance) :
  kodi::addon::CInstancePVRClient(instance),
  m_state(PVR_CONNECTION_STATE_UNKNOWN)
{
  m_iCurrentChannel        = -1;
  m_bCurrentChannelIsRadio = false;
  m_iCurrentCard           = -1;
  m_tcpclient              = new MPTV::Socket(MPTV::af_unspec, MPTV::pf_inet, MPTV::sock_stream, MPTV::tcp);
  m_bStop                  = true;
  m_bTimeShiftStarted      = false;
  m_bSkipCloseLiveStream   = false;
  m_BackendUTCoffset       = 0;
  m_BackendTime            = 0;
  m_tsreader               = NULL;
  m_genretable             = NULL;
  m_signalStateCounter     = 0;
  m_iSignal                = 0;
  m_iSNR                   = 0;
  m_lastSelectedRecording  = NULL;

  /* Generate the recording life time strings */
  Timer::lifetimeValues = new cLifeTimeValues();
}

cPVRClientMediaPortal::~cPVRClientMediaPortal()
{
  kodi::Log(ADDON_LOG_DEBUG, "->~cPVRClientMediaPortal()");
  Disconnect();

  SafeDelete(Timer::lifetimeValues);
  SafeDelete(m_tcpclient);
  SafeDelete(m_genretable);
  SafeDelete(m_lastSelectedRecording);
}

string cPVRClientMediaPortal::SendCommand(const char* command)
{
  std::string cmd(command);
  return SendCommand(cmd);
}

string cPVRClientMediaPortal::SendCommand(const string& command)
{
  std::lock_guard<std::mutex> critsec(m_mutex);

  if ( !m_tcpclient->send(command) )
  {
    if ( !m_tcpclient->is_valid() )
    {
      SetConnectionState(PVR_CONNECTION_STATE_DISCONNECTED);

      // Connection lost, try to reconnect
      if (TryConnect() == ADDON_STATUS_OK)
      {
        // Resend the command
        if (!m_tcpclient->send(command))
        {
          kodi::Log(ADDON_LOG_ERROR, "SendCommand('%s') failed.", command.c_str());
          return "";
        }
      }
      else
      {
        kodi::Log(ADDON_LOG_ERROR, "SendCommand: reconnect failed.");
        return "";
      }
    }
  }

  string result;

  if ( !m_tcpclient->ReadLine( result ) )
  {
    kodi::Log(ADDON_LOG_ERROR, "SendCommand - Failed.");
    return "";
  }

  if (result.find("[ERROR]:") != std::string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "TVServerKodi error: %s", result.c_str());
  }

  return result;
}


bool cPVRClientMediaPortal::SendCommand2(const string& command, vector<string>& lines)
{
  string result = SendCommand(command);

  if (result.empty())
  {
    return false;
  }

  Tokenize(result, lines, ",");

  return true;
}

ADDON_STATUS cPVRClientMediaPortal::TryConnect()
{
  /* Open Connection to MediaPortal Backend TV Server via the TVServerKodi plugin */
  kodi::Log(ADDON_LOG_INFO, "Mediaportal pvr addon " STR(MPTV_VERSION) " connecting to %s:%i", CSettings::Get().GetHostname().c_str(), CSettings::Get().GetPort());

  PVR_CONNECTION_STATE result = Connect();

  switch (result)
  {
    case PVR_CONNECTION_STATE_ACCESS_DENIED:
    case PVR_CONNECTION_STATE_UNKNOWN:
    case PVR_CONNECTION_STATE_SERVER_MISMATCH:
    case PVR_CONNECTION_STATE_VERSION_MISMATCH:
      return ADDON_STATUS_PERMANENT_FAILURE;
    case PVR_CONNECTION_STATE_DISCONNECTED:
    case PVR_CONNECTION_STATE_SERVER_UNREACHABLE:
      kodi::Log(ADDON_LOG_ERROR, "Could not connect to MediaPortal TV Server backend.");
      // Start background thread for connecting to the backend
      if (!m_running)
      {
        m_running = true;
        m_thread = std::thread([&] { Process(); });
      }
      return ADDON_STATUS_LOST_CONNECTION;
    case PVR_CONNECTION_STATE_CONNECTING:
    case PVR_CONNECTION_STATE_CONNECTED:
      break;
  }

  return ADDON_STATUS_OK;
}

PVR_CONNECTION_STATE cPVRClientMediaPortal::Connect(bool updateConnectionState)
{
  std::lock_guard<std::mutex> critsec(m_connectionMutex);

  string result;

  if (!m_tcpclient->create())
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not connect create socket");
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_UNKNOWN);
    }
    return PVR_CONNECTION_STATE_UNKNOWN;
  }
  if (updateConnectionState)
  {
    SetConnectionState(PVR_CONNECTION_STATE_CONNECTING);
  }

  if (!m_tcpclient->connect(CSettings::Get().GetHostname(), (unsigned short) CSettings::Get().GetPort()))
  {
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_SERVER_UNREACHABLE);
    }
    return PVR_CONNECTION_STATE_SERVER_UNREACHABLE;
  }

  m_tcpclient->set_non_blocking(1);
  kodi::Log(ADDON_LOG_INFO, "Connected to %s:%i", CSettings::Get().GetHostname().c_str(), CSettings::Get().GetPort());

  result = SendCommand("PVRclientXBMC:0-1\n");

  if (result.length() == 0)
  {
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_UNKNOWN);
    }
    return PVR_CONNECTION_STATE_UNKNOWN;
  }

  if(result.find("Unexpected protocol") != std::string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "TVServer does not accept protocol: PVRclientXBMC:0-1");
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_SERVER_MISMATCH);
    }
    return PVR_CONNECTION_STATE_SERVER_MISMATCH;
  }

  vector<string> fields;
  int major = 0, minor = 0, revision = 0;

  // Check the version of the TVServerKodi plugin:
  Tokenize(result, fields, "|");
  if(fields.size() < 2)
  {
    kodi::Log(ADDON_LOG_ERROR, "Your TVServerKodi version is too old. Please upgrade to '%s' or higher!", TVSERVERKODI_MIN_VERSION_STRING);
    kodi::QueueFormattedNotification(QUEUE_ERROR, kodi::addon::GetLocalizedString(30051).c_str(), TVSERVERKODI_MIN_VERSION_STRING);
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
    }
    return PVR_CONNECTION_STATE_VERSION_MISMATCH;
  }

  // Ok, this TVServerKodi version answers with a version string
  int count = sscanf(fields[1].c_str(), "%5d.%5d.%5d.%5d", &major, &minor, &revision, &g_iTVServerKodiBuild);
  if( count < 4 )
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not parse the TVServerKodi version string '%s'", fields[1].c_str());
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
    }
    return PVR_CONNECTION_STATE_VERSION_MISMATCH;
  }

  // Check for the minimal requirement: 1.1.0.70
  if( g_iTVServerKodiBuild < TVSERVERKODI_MIN_VERSION_BUILD ) //major < 1 || minor < 1 || revision < 0 || build < 70
  {
    kodi::Log(ADDON_LOG_ERROR, "Your TVServerKodi version '%s' is too old. Please upgrade to '%s' or higher!", fields[1].c_str(), TVSERVERKODI_MIN_VERSION_STRING);
    kodi::QueueFormattedNotification(QUEUE_ERROR, kodi::addon::GetLocalizedString(30051).c_str(), fields[1].c_str(), TVSERVERKODI_MIN_VERSION_STRING);
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
    }
    return PVR_CONNECTION_STATE_VERSION_MISMATCH;
  }
  else
  {
    kodi::Log(ADDON_LOG_INFO, "Your TVServerKodi version is '%s'", fields[1].c_str());

    // Advice to upgrade:
    if( g_iTVServerKodiBuild < TVSERVERKODI_RECOMMENDED_VERSION_BUILD )
    {
      kodi::Log(ADDON_LOG_INFO, "It is advised to upgrade your TVServerKodi version '%s' to '%s' or higher!", fields[1].c_str(), TVSERVERKODI_RECOMMENDED_VERSION_STRING);
    }
  }

  /* Store connection string */
  char buffer[512];
  snprintf(buffer, 512, "%s:%i", CSettings::Get().GetHostname().c_str(), CSettings::Get().GetPort());
  m_ConnectionString = buffer;

  if (updateConnectionState)
  {
    SetConnectionState(PVR_CONNECTION_STATE_CONNECTED);
  }

  /* Load additional settings */
  LoadGenreTable();
  LoadCardSettings();

  /* The pvr addon cannot access Kodi's current locale settings, so just use the system default */
  setlocale(LC_ALL, "");

  return PVR_CONNECTION_STATE_CONNECTED;
}

void cPVRClientMediaPortal::Disconnect()
{
  string result;

  kodi::Log(ADDON_LOG_INFO, "Disconnect");

  if (m_running)
  {
    m_running = false;
    if (m_thread.joinable())
      m_thread.join();
  }

  if (m_tcpclient->is_valid() && m_bTimeShiftStarted)
  {
    result = SendCommand("IsTimeshifting:\n");

    if (result.find("True") != std::string::npos )
    {
      if ((CSettings::Get().GetStreamingMethod()==TSReader) && (m_tsreader != NULL))
      {
        m_tsreader->Close();
        SafeDelete(m_tsreader);
      }
      SendCommand("StopTimeshift:\n");
    }
  }

  m_bStop = true;

  m_tcpclient->close();

  SetConnectionState(PVR_CONNECTION_STATE_DISCONNECTED);
}

/* IsUp()
 * \brief   Check whether we still have a connection with the TVServer. If not, try
 *          to reconnect
 * \return  True when a connection is available, False when even a reconnect failed
 */
bool cPVRClientMediaPortal::IsUp()
{
  if (m_state == PVR_CONNECTION_STATE_CONNECTED)
  {
      return true;
  }
  else
  {
    return false;
  }
}

void cPVRClientMediaPortal::Process()
{
  kodi::Log(ADDON_LOG_DEBUG, "Background thread started.");

  bool keepWaiting = true;
  PVR_CONNECTION_STATE state;

  while (m_running && keepWaiting)
  {
    state = Connect(false);

    switch (state)
    {
    case PVR_CONNECTION_STATE_ACCESS_DENIED:
    case PVR_CONNECTION_STATE_UNKNOWN:
    case PVR_CONNECTION_STATE_SERVER_MISMATCH:
    case PVR_CONNECTION_STATE_VERSION_MISMATCH:
      keepWaiting = false;
      break;
    case PVR_CONNECTION_STATE_CONNECTED:
      keepWaiting = false;
      break;
    default:
      break;
    }

    if (keepWaiting)
    {
      // Wait for 1 minute before re-trying
      std::this_thread::sleep_for(std::chrono::milliseconds(60000));
    }
  }
  SetConnectionState(state);

  kodi::Log(ADDON_LOG_DEBUG, "Background thread finished.");
}


/************************************************************/
/** General handling */

PVR_ERROR cPVRClientMediaPortal::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  kodi::Log(ADDON_LOG_DEBUG, "->GetCapabilities()");

  capabilities.SetSupportsEPG(true);
  capabilities.SetSupportsEPGEdl(false);
  capabilities.SetSupportsTV(true);
  capabilities.SetSupportsRadio(CSettings::Get().GetRadioEnabled());
  capabilities.SetSupportsRecordings(true);
  capabilities.SetSupportsRecordingsDelete(true);
  capabilities.SetSupportsRecordingsUndelete(false);
  capabilities.SetSupportsTimers(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetSupportsChannelScan(false);
  capabilities.SetSupportsChannelSettings(false);
  capabilities.SetHandlesInputStream(true);
  capabilities.SetHandlesDemuxing(false);
  capabilities.SetSupportsRecordingPlayCount((g_iTVServerKodiBuild < 117) ? false : true);
  capabilities.SetSupportsLastPlayedPosition((g_iTVServerKodiBuild < 121) ? false : true);
  capabilities.SetSupportsRecordingEdl(true);
  capabilities.SetSupportsRecordingsRename(true);
  capabilities.SetSupportsRecordingsLifetimeChange(false);
  capabilities.SetSupportsDescrambleInfo(false);
  capabilities.SetSupportsAsyncEPGTransfer(false);

  return PVR_ERROR_NO_ERROR;
}

// Used among others for the server name string in the "Recordings" view
PVR_ERROR cPVRClientMediaPortal::GetBackendName(std::string& name)
{
  if (!IsUp())
  {
    name = CSettings::Get().GetHostname();
    return PVR_ERROR_NO_ERROR;
  }

  kodi::Log(ADDON_LOG_DEBUG, "->GetBackendName()");

  if (m_BackendName.length() == 0)
  {
    m_BackendName = "MediaPortal TV-server (";
    m_BackendName += SendCommand("GetBackendName:\n");
    m_BackendName += ")";
  }

  name = m_BackendName;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetBackendVersion(std::string& version)
{
  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(m_BackendVersion.length() == 0)
  {
    m_BackendVersion = SendCommand("GetVersion:\n");
  }

  kodi::Log(ADDON_LOG_DEBUG, "GetBackendVersion: %s", m_BackendVersion.c_str());

  version = m_BackendVersion;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetConnectionString(std::string& connection)
{
  if (m_ConnectionString.empty())
    return PVR_ERROR_NO_ERROR;

  kodi::Log(ADDON_LOG_DEBUG, "GetConnectionString: %s", m_ConnectionString.c_str());
  connection = m_ConnectionString;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetDriveSpace(uint64_t& total, uint64_t& used)
{
  string result;
  vector<string> fields;

  total = 0;
  used = 0;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetDriveSpace:\n");

  Tokenize(result, fields, "|");

  if(fields.size() >= 2)
  {
    total = std::stoll(fields[0]);
    used = std::stoll(fields[1]);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetBackendTime(time_t *localTime, int *gmtOffset)
{
  string result;
  vector<string> fields;
  int year = 0, month = 0, day = 0;
  int hour = 0, minute = 0, second = 0;
  struct tm timeinfo;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetTime:\n");

  if (result.length() == 0)
    return PVR_ERROR_SERVER_ERROR;

  Tokenize(result, fields, "|");

  if(fields.size() >= 3)
  {
    //[0] date + time TV Server
    //[1] UTC offset hours
    //[2] UTC offset minutes
    //From CPVREpg::CPVREpg(): Expected PVREpg GMT offset is in seconds
    m_BackendUTCoffset = ((atoi(fields[1].c_str()) * 60) + atoi(fields[2].c_str())) * 60;

    int count = sscanf(fields[0].c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

    if(count == 6)
    {
      //timeinfo = *localtime ( &rawtime );
      kodi::Log(ADDON_LOG_DEBUG, "GetMPTVTime: time from MP TV Server: %d-%d-%d %d:%d:%d, offset %d seconds", year, month, day, hour, minute, second, m_BackendUTCoffset );
      timeinfo.tm_hour = hour;
      timeinfo.tm_min = minute;
      timeinfo.tm_sec = second;
      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon = month - 1;
      timeinfo.tm_mday = day;
      timeinfo.tm_isdst = -1; //Actively determines whether DST is in effect from the specified time and the local time zone.
      // Make the other fields empty:
      timeinfo.tm_wday = 0;
      timeinfo.tm_yday = 0;

      m_BackendTime = mktime(&timeinfo);

      if(m_BackendTime < 0)
      {
        kodi::Log(ADDON_LOG_DEBUG, "GetMPTVTime: Unable to convert string '%s' into date+time", fields[0].c_str());
        return PVR_ERROR_SERVER_ERROR;
      }

      kodi::Log(ADDON_LOG_DEBUG, "GetMPTVTime: localtime %s", asctime(localtime(&m_BackendTime)));
      kodi::Log(ADDON_LOG_DEBUG, "GetMPTVTime: gmtime    %s", asctime(gmtime(&m_BackendTime)));

      *localTime = m_BackendTime;
      *gmtOffset = m_BackendUTCoffset;
      return PVR_ERROR_NO_ERROR;
    }
    else
    {
      return PVR_ERROR_SERVER_ERROR;
    }
  }
  else
    return PVR_ERROR_SERVER_ERROR;
}

/************************************************************/
/** EPG handling */

namespace
{

std::string ParseAsW3CDateString(time_t time)
{
  std::tm* tm = std::localtime(&time);
  char buffer[16];
  std::strftime(buffer, 16, "%Y-%m-%d", tm);

  return buffer;
}

} // unnamed namespace

PVR_ERROR cPVRClientMediaPortal::GetEPGForChannel(int channelUid, time_t start, time_t end, kodi::addon::PVREPGTagsResultSet& results)
{
  vector<string> lines;
  char           command[256];
  string         result;
  cEpg           epg;
  struct tm      starttime;
  struct tm      endtime;

  starttime = *gmtime( &start );
  endtime = *gmtime( &end );

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // Request (extended) EPG data for the given period
  snprintf(command, 256, "GetEPG:%i|%04d-%02d-%02dT%02d:%02d:%02d.0Z|%04d-%02d-%02dT%02d:%02d:%02d.0Z\n",
          channelUid,                                                       //Channel id
          starttime.tm_year + 1900, starttime.tm_mon + 1, starttime.tm_mday, //Start date     [2..4]
          starttime.tm_hour, starttime.tm_min, starttime.tm_sec,             //Start time     [5..7]
          endtime.tm_year + 1900, endtime.tm_mon + 1, endtime.tm_mday,       //End date       [8..10]
          endtime.tm_hour, endtime.tm_min, endtime.tm_sec);                  //End time       [11..13]

  result = SendCommand(command);
  if(result.compare(0,5, "ERROR") != 0)
  {
    if( result.length() != 0)
    {
      epg.SetGenreTable(m_genretable);

      Tokenize(result, lines, ",");

      kodi::Log(ADDON_LOG_DEBUG, "Found %i EPG items for channel %i\n", lines.size(), channelUid);

      for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
      {
        string& data(*it);

        if( data.length() > 0)
        {
          uri::decode(data);

          bool isEnd = epg.ParseLine(data);

          if (isEnd && epg.StartTime() != 0)
          {
            kodi::addon::PVREPGTag broadcast;

            broadcast.SetUniqueBroadcastId(epg.UniqueId());
            broadcast.SetTitle(epg.Title());
            broadcast.SetUniqueChannelId(channelUid);
            broadcast.SetStartTime(epg.StartTime());
            broadcast.SetEndTime(epg.EndTime());
            broadcast.SetPlotOutline(epg.PlotOutline());
            broadcast.SetPlot(epg.Description());
            broadcast.SetIconPath("");
            broadcast.SetGenreType(epg.GenreType());
            broadcast.SetGenreSubType(epg.GenreSubType());
            broadcast.SetGenreDescription(epg.Genre());
            std::string strFirstAired(epg.OriginalAirDate() > 0 ? ParseAsW3CDateString(epg.OriginalAirDate()) : "");
            broadcast.SetFirstAired(strFirstAired.c_str());
            broadcast.SetParentalRating(epg.ParentalRating());
            broadcast.SetStarRating(epg.StarRating());
            broadcast.SetSeriesNumber(epg.SeriesNumber());
            broadcast.SetEpisodeNumber(epg.EpisodeNumber());
            broadcast.SetEpisodePartNumber(atoi(epg.EpisodePart()));
            broadcast.SetEpisodeName(epg.EpisodeName());
            broadcast.SetFlags(EPG_TAG_FLAG_UNDEFINED);

            results.Add(broadcast);
          }
          epg.Reset();
        }
      }
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "No EPG items found for channel %i", channelUid);
    }
  }
  else
  {
    kodi::Log(ADDON_LOG_DEBUG, "RequestEPGForChannel(%i) %s", channelUid, result.c_str());
  }

  return PVR_ERROR_NO_ERROR;
}


/************************************************************/
/** Channel handling */

PVR_ERROR cPVRClientMediaPortal::GetChannelsAmount(int& amount)
{
  string result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // Get the total channel count (radio+tv)
  // It is only used to check whether Kodi should request the channel list
  result = SendCommand("GetChannelCount:\n");

  amount = atol(result.c_str());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  vector<string>  lines;
  std::string     command;
  const char *    baseCommand;
  std::string     stream;
  std::string     groups;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(radio)
  {
    if(!CSettings::Get().GetRadioEnabled())
    {
      kodi::Log(ADDON_LOG_INFO, "Fetching radio channels is disabled.");
      return PVR_ERROR_NO_ERROR;
    }

    baseCommand = "ListRadioChannels";
    if (CSettings::Get().GetRadioGroup().empty())
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannels(radio) all channels");
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannels(radio) for radio group(s): '%s'", CSettings::Get().GetRadioGroup().c_str());
      groups = uri::encode(uri::PATH_TRAITS, CSettings::Get().GetRadioGroup());
      StringUtils::Replace(groups, "%7C","|");
    }
  }
  else
  {
    baseCommand = "ListTVChannels";
    if (CSettings::Get().GetTVGroup().empty())
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannels(tv) all channels");
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannels(tv) for TV group(s): '%s'", CSettings::Get().GetTVGroup().c_str());
      groups = uri::encode(uri::PATH_TRAITS, CSettings::Get().GetTVGroup());
      StringUtils::Replace(groups, "%7C","|");
    }
  }

  if (groups.empty())
    command = StringUtils::Format("%s\n", baseCommand);
  else
    command = StringUtils::Format("%s:%s\n", baseCommand, groups.c_str());

  if( !SendCommand2(command, lines) )
    return PVR_ERROR_SERVER_ERROR;

#ifdef TARGET_WINDOWS_DESKTOP
  bool bCheckForThumbs = false;
  /* Check if we can find the MediaPortal channel logo folders on this machine */
  std::string strThumbPath;
  std::string strProgramData;

  if (OS::GetProgramData(strProgramData) == true)
  {
    strThumbPath = strProgramData + "\\Team MediaPortal\\MediaPortal\\Thumbs\\";
    if (radio)
      strThumbPath += "Radio\\";
    else
      strThumbPath += "TV\\logos\\";

    bCheckForThumbs = OS::CFile::Exists(strThumbPath);
  }
#endif // TARGET_WINDOWS_DESKTOP


  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      if(radio)
        kodi::Log(ADDON_LOG_DEBUG, "TVServer returned no data. Empty/non existing radio group '%s'?", CSettings::Get().GetRadioGroup().c_str());
      else
        kodi::Log(ADDON_LOG_DEBUG, "TVServer returned no data. Empty/non existing tv group '%s'?", CSettings::Get().GetTVGroup().c_str());
      break;
    }

    uri::decode(data);

    cChannel channel;
    if( channel.Parse(data) )
    {
      // Cache this channel in our local uid-channel list
      // This cache is used for the GUIDialogRecordSettings
      m_channels[channel.UID()] = channel;

      kodi::addon::PVRChannel tag;

      // Prepare the PVR_CHANNEL struct to transfer this channel to Kodi
      tag.SetUniqueId(channel.UID());
      if (channel.MajorChannelNr() == -1)
      {
        tag.SetChannelNumber(channel.ExternalID());
      }
      else
      {
        tag.SetChannelNumber(channel.MajorChannelNr());
        tag.SetSubChannelNumber(channel.MinorChannelNr());
      }
      tag.SetChannelName(channel.Name());
      tag.SetIconPath("");
#ifdef TARGET_WINDOWS_DESKTOP
      if (bCheckForThumbs)
      {
        const int ciExtCount = 5;
        string strIconExt [ciExtCount] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };
        string strIconName;
        string strIconBaseName;

        kodi::Log(ADDON_LOG_DEBUG, "Checking for a channel thumbnail for channel %s in %s", channel.Name(), strThumbPath.c_str());

        strIconBaseName = strThumbPath + ToThumbFileName(channel.Name());

        for (int i=0; i < ciExtCount; i++)
        {
          strIconName = strIconBaseName + strIconExt[i];
          if ( OS::CFile::Exists(strIconName) )
          {
            tag.SetIconPath(strIconName);
            kodi::Log(ADDON_LOG_DEBUG, "Found channel thumb: %s", tag.GetIconPath().c_str());
            break;
          }
        }
      }
#endif
      tag.SetEncryptionSystem(channel.Encrypted());
      tag.SetIsRadio(radio);
      tag.SetIsHidden(!channel.VisibleInGuide());

      if(channel.IsWebstream())
      {
        kodi::Log(ADDON_LOG_DEBUG, "Channel '%s' has a webstream: %s. TODO fixme.", channel.Name(), channel.URL());
        tag.SetMimeType("");
      }
      else
      {
        if (CSettings::Get().GetStreamingMethod()==TSReader)
        {
          // TSReader
          //Use OpenLiveStream to read from the timeshift .ts file or an rtsp stream
          if (!radio)
            tag.SetMimeType("video/mp2t");
          else
            tag.SetMimeType("");
        }
        else
        {
          //Use GetLiveStreamURL to fetch an rtsp stream
          kodi::Log(ADDON_LOG_DEBUG, "Channel '%s' has a rtsp stream: %s. TODO fixme.", channel.Name(), channel.URL());
          tag.SetMimeType("");
        }
      }

      if( (!CSettings::Get().GetOnlyFTA()) || (tag.GetEncryptionSystem()==0))
      {
        results.Add(tag);
      }
    }
  }

  //pthread_mutex_unlock(&m_critSection);
  return PVR_ERROR_NO_ERROR;
}

/************************************************************/
/** Channel group handling **/

PVR_ERROR cPVRClientMediaPortal::GetChannelGroupsAmount(int& amount)
{
  // Not directly possible at the moment
  kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroupsAmount: TODO");

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // just tell Kodi that we have groups
  amount = 1;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  vector<string>  lines;
  std::string   filters;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(radio)
  {
    if (!CSettings::Get().GetRadioEnabled())
    {
      kodi::Log(ADDON_LOG_DEBUG, "Skipping GetChannelGroups for radio. Radio support is disabled.");
      return PVR_ERROR_NO_ERROR;
    }

    filters = CSettings::Get().GetRadioGroup();

    kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroups for radio");
    if (!SendCommand2("ListRadioGroups\n", lines))
      return PVR_ERROR_SERVER_ERROR;
  }
  else
  {
    filters = CSettings::Get().GetTVGroup();

    kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroups for TV");
    if (!SendCommand2("ListGroups\n", lines))
      return PVR_ERROR_SERVER_ERROR;
  }

  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      kodi::Log(ADDON_LOG_DEBUG, "TVServer returned no data. No %s groups found?", ((radio) ? "radio" : "tv"));
      break;
    }

    uri::decode(data);

    if (data.compare("All Channels") == 0)
    {
      kodi::Log(ADDON_LOG_DEBUG, "Skipping All Channels (%s) group", ((radio) ? "radio" : "tv"));
    }
    else
    {
      if (!filters.empty())
      {
        if (filters.find(data.c_str()) == string::npos)
        {
          // Skip this backend group. It is not in our filter list
          continue;
        }
      }

      kodi::addon::PVRChannelGroup tag;
      tag.SetIsRadio(radio);
      tag.SetGroupName(data);
      kodi::Log(ADDON_LOG_DEBUG, "Adding %s group: %s", ((radio) ? "radio" : "tv"), tag.GetGroupName().c_str());
      results.Add(tag);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group, kodi::addon::PVRChannelGroupMembersResultSet& results)
{
  //TODO: code below is similar to GetChannels code. Refactor and combine...
  vector<string>           lines;
  std::string              command;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if (group.GetIsRadio())
  {
    if (CSettings::Get().GetRadioEnabled())
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroupMembers: for radio group '%s'", group.GetGroupName().c_str());
      command = StringUtils::Format("ListRadioChannels:%s\n", uri::encode(uri::PATH_TRAITS, group.GetGroupName()).c_str());
    }
    else
    {
      kodi::Log(ADDON_LOG_DEBUG, "Skipping GetChannelGroupMembers for radio. Radio support is disabled.");
      return PVR_ERROR_NO_ERROR;
    }
  }
  else
  {
    kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroupMembers: for tv group '%s'", group.GetGroupName().c_str());
    command = StringUtils::Format("ListTVChannels:%s\n", uri::encode(uri::PATH_TRAITS, group.GetGroupName()).c_str());
  }

  if (!SendCommand2(command, lines))
    return PVR_ERROR_SERVER_ERROR;

  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      if(group.GetIsRadio())
        kodi::Log(ADDON_LOG_DEBUG, "TVServer returned no data. Empty/non existing radio group '%s'?", CSettings::Get().GetRadioGroup().c_str());
      else
        kodi::Log(ADDON_LOG_DEBUG, "TVServer returned no data. Empty/non existing tv group '%s'?", CSettings::Get().GetTVGroup().c_str());
      break;
    }

    uri::decode(data);

    cChannel channel;
    if( channel.Parse(data) )
    {
      kodi::addon::PVRChannelGroupMember tag;

      tag.SetChannelUniqueId(channel.UID());
      if (channel.MajorChannelNr() == -1)
      {
        tag.SetChannelNumber(channel.ExternalID());
      }
      else
      {
        tag.SetChannelNumber(channel.MajorChannelNr());
        tag.SetSubChannelNumber(channel.MinorChannelNr());
      }
      tag.SetGroupName(group.GetGroupName());


      // Don't add encrypted channels when FTA only option is turned on
      if( (!CSettings::Get().GetOnlyFTA()) || (channel.Encrypted()==false))
      {
        kodi::Log(ADDON_LOG_DEBUG, "GetChannelGroupMembers: add channel %s to group '%s' (Backend channel uid=%d, channelnr=%d)",
          channel.Name(), group.GetGroupName().c_str(), tag.GetChannelUniqueId(), tag.GetChannelNumber());
        results.Add(tag);
      }
    }
  }

  return PVR_ERROR_NO_ERROR;
}

/************************************************************/
/** Record handling **/

PVR_ERROR cPVRClientMediaPortal::GetRecordingsAmount(bool deleted, int& amount)
{
  string            result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetRecordingCount:\n");

  amount = atol(result.c_str());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results)
{
  vector<string>  lines;
  string          result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(CSettings::Get().GetResolveRTSPHostname() == false)
  {
    result = SendCommand("ListRecordings:False\n");
  }
  else
  {
    result = SendCommand("ListRecordings\n");
  }

  if( result.length() == 0 )
  {
    kodi::Log(ADDON_LOG_DEBUG, "Backend returned no recordings" );
    return PVR_ERROR_NO_ERROR;
  }

  Tokenize(result, lines, ",");

  for (vector<string>::iterator it = lines.begin(); it != lines.end(); ++it)
  {
    string& data(*it);
    uri::decode(data);

    kodi::Log(ADDON_LOG_DEBUG, "RECORDING: %s", data.c_str() );

    std::string strRecordingId;
    std::string strDirectory;
    std::string strEpisodeName;
    cRecording recording;

    recording.SetCardSettings(&m_cCards);
    recording.SetGenreTable(m_genretable);

    if (recording.ParseLine(data))
    {
      strRecordingId = StringUtils::Format("%i", recording.Index());
      strEpisodeName = recording.EpisodeName();

      kodi::addon::PVRRecording tag;

      tag.SetRecordingId(strRecordingId);
      tag.SetTitle(recording.Title());
      tag.SetEpisodeName(recording.EpisodeName());
      tag.SetPlot(recording.Description());
      tag.SetChannelName(recording.ChannelName());
      tag.SetChannelUid(recording.ChannelID());
      tag.SetRecordingTime(recording.StartTime());
      tag.SetDuration((int) recording.Duration());
      tag.SetPriority(0); // only available for schedules, not for recordings
      tag.SetLifetime(recording.Lifetime());
      tag.SetGenreType(recording.GenreType());
      tag.SetGenreSubType(recording.GenreSubType());
      tag.SetGenreDescription(recording.GetGenre());
      tag.SetPlayCount(recording.TimesWatched());
      tag.SetLastPlayedPosition(recording.LastPlayedPosition());
      tag.SetEpisodeNumber(recording.GetEpisodeNumber());
      tag.SetSeriesNumber(recording.GetSeriesNumber());
      tag.SetEPGEventId(EPG_TAG_INVALID_UID);
      tag.SetChannelType(recording.GetChannelType());

      strDirectory = recording.Directory();
      if (strDirectory.length() > 0)
      {
        StringUtils::Replace(strDirectory, "\\", " - "); // Kodi supports only 1 sublevel below Recordings, so flatten the MediaPortal directory structure
        tag.SetDirectory(strDirectory); // used in Kodi as directory structure below "Recordings"
        if ((StringUtils::EqualsNoCase(strDirectory, tag.GetTitle())) && (strEpisodeName.length() > 0))
        {
          strEpisodeName = recording.Title();
          strEpisodeName+= " - ";
          strEpisodeName+= recording.EpisodeName();
          tag.SetTitle(strEpisodeName);
        }
      }
      else
      {
        tag.SetDirectory("");
      }

      tag.SetThumbnailPath("");

#ifdef TARGET_WINDOWS_DESKTOP
      std::string recordingUri(ToKodiPath(recording.FilePath()));
      if (CSettings::Get().GetUseRTSP() == false)
      {
        /* Recording thumbnail */
        std::string strThumbnailName(recordingUri);
        StringUtils::Replace(strThumbnailName, ".ts", ".jpg");
        /* Check if it exists next to the recording */
        if (kodi::vfs::FileExists(strThumbnailName, false))
        {
          tag.SetThumbnailPath(strThumbnailName);
        }
        else
        {
          /* Check also: C:\ProgramData\Team MediaPortal\MediaPortal TV Server\thumbs */
          std::string strThumbnailFilename = recording.FileName();
          StringUtils::Replace(strThumbnailFilename, ".ts", ".jpg");
          std::string strProgramData;
          if (OS::GetProgramData(strProgramData))
          {
            /* MediaPortal 1 */
            strThumbnailName = strProgramData +
                               "\\Team MediaPortal\\MediaPortal TV Server\\thumbs\\" +
                               strThumbnailFilename;
            if (kodi::vfs::FileExists(strThumbnailName, false))
            {
              tag.SetThumbnailPath(strThumbnailName);
            }
            else
            {
              /* MediaPortal 2 */
              strThumbnailName = strProgramData +
                                 "\\Team MediaPortal\\MP2-Server\\SlimTVCore\\v3.0\\thumbs\\" +
                                 strThumbnailFilename;
              if (kodi::vfs::FileExists(strThumbnailName, false))
              {
                tag.SetThumbnailPath(strThumbnailName);
              }
            }
          }
          else
            tag.SetThumbnailPath("");
        }
      }
#endif /* TARGET_WINDOWS_DESKTOP */

      if (CSettings::Get().GetStreamingMethod()!=TSReader)
      {
        // Use rtsp url and Kodi's internal FFMPeg playback
        kodi::Log(ADDON_LOG_DEBUG, "Recording '%s' has a rtsp url '%s'. TODO Fix me. ", recording.Title(), recording.Stream());
      }

      results.Add(tag);
    }
  }

  m_iLastRecordingUpdate = std::chrono::system_clock::now();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
  char            command[1200];
  string          result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 1200, "DeleteRecordedTV:%s\n", recording.GetRecordingId().c_str());

  result = SendCommand(command);

  if(result.find("True") ==  string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "Deleting recording %s [failed]", recording.GetRecordingId().c_str());
    return PVR_ERROR_FAILED;
  }
  kodi::Log(ADDON_LOG_DEBUG, "Deleting recording %s [done]", recording.GetRecordingId().c_str());

  // Although Kodi initiates the deletion of this recording, we still have to trigger Kodi to update its
  // recordings list to remove the recording at the Kodi side
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::RenameRecording(const kodi::addon::PVRRecording& recording)
{
  char           command[1200];
  string         result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 1200, "UpdateRecording:%s|%s\n",
    recording.GetRecordingId().c_str(),
    uri::encode(uri::PATH_TRAITS, recording.GetTitle()).c_str());

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "RenameRecording(%s) to %s [failed]", recording.GetRecordingId().c_str(), recording.GetTitle().c_str());
    return PVR_ERROR_FAILED;
  }
  kodi::Log(ADDON_LOG_DEBUG, "RenameRecording(%s) to %s [done]", recording.GetRecordingId().c_str(), recording.GetTitle().c_str());

  // Although Kodi initiates the rename of this recording, we still have to trigger Kodi to update its
  // recordings list to see the renamed recording at the Kodi side
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count)
{
  if ( g_iTVServerKodiBuild < 117 )
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  char           command[512];
  string         result;

  snprintf(command, 512, "SetRecordingTimesWatched:%i|%i\n", std::atoi(recording.GetRecordingId().c_str()), count);

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: id=%s to %i [failed]", __FUNCTION__, recording.GetRecordingId().c_str(), count);
    return PVR_ERROR_FAILED;
  }

  kodi::Log(ADDON_LOG_DEBUG, "%s: id=%s to %i [successful]", __FUNCTION__, recording.GetRecordingId().c_str(), count);
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int lastplayedposition)
{
  if ( g_iTVServerKodiBuild < 121 )
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  char           command[512];
  string         result;

  if (lastplayedposition < 0)
  {
    lastplayedposition = 0;
  }

  snprintf(command, 512, "SetRecordingStopTime:%i|%i\n", std::atoi(recording.GetRecordingId().c_str()), lastplayedposition);

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: id=%s to %i [failed]", __FUNCTION__, recording.GetRecordingId().c_str(), lastplayedposition);
    return PVR_ERROR_FAILED;
  }

  kodi::Log(ADDON_LOG_DEBUG, "%s: id=%s to %i [successful]", __FUNCTION__, recording.GetRecordingId().c_str(), lastplayedposition);
  kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position)
{
  if ( g_iTVServerKodiBuild < 121 )
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  char           command[512];
  string         result;

  snprintf(command, 512, "GetRecordingStopTime:%i\n", std::atoi(recording.GetRecordingId().c_str()));

  result = SendCommand(command);

  if(result.find("-1") != string::npos)
  {
    kodi::Log(ADDON_LOG_ERROR, "%s: id=%s fetching stoptime [failed]", __FUNCTION__, recording.GetRecordingId().c_str());
    return PVR_ERROR_UNKNOWN;
  }

  position = std::atoi(result.c_str());

  kodi::Log(ADDON_LOG_DEBUG, "%s: id=%s stoptime=%i {s} [successful]", __FUNCTION__, recording.GetRecordingId().c_str(), position);

  return PVR_ERROR_NO_ERROR;
}

/************************************************************/
/** Timer handling */

PVR_ERROR cPVRClientMediaPortal::GetTimersAmount(int& amount)
{
  string            result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetScheduleCount:\n");

  amount = std::stol(result);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetTimers(kodi::addon::PVRTimersResultSet& results)
{
  vector<string>  lines;
  string          result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("ListSchedules:True\n");

  if (result.length() > 0)
  {
    Tokenize(result, lines, ",");

    for (vector<string>::iterator it = lines.begin(); it != lines.end(); ++it)
    {
      string& data(*it);
      uri::decode(data);

      kodi::Log(ADDON_LOG_DEBUG, "SCHEDULED: %s", data.c_str() );

      cTimer timer;
      timer.SetGenreTable(m_genretable);

      if(timer.ParseLine(data.c_str()) == true)
      {
        kodi::addon::PVRTimer tag;
        timer.GetPVRtimerinfo(tag);
        results.Add(tag);
      }
    }
  }

  if ( std::chrono::system_clock::now() >  m_iLastRecordingUpdate + std::chrono::milliseconds(15000))
  {
    kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetTimerInfo(unsigned int timernumber, kodi::addon::PVRTimer& timerinfo)
{
  string         result;
  char           command[256];

  kodi::Log(ADDON_LOG_DEBUG, "->GetTimerInfo(%u)", timernumber);

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 256, "GetScheduleInfo:%u\n", timernumber);

  result = SendCommand(command);

  if (result.length() == 0)
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer;
  if( timer.ParseLine(result.c_str()) == false )
  {
    kodi::Log(ADDON_LOG_DEBUG, "GetTimerInfo(%i) parsing server response failed. Response: %s", timernumber, result.c_str());
    return PVR_ERROR_SERVER_ERROR;
  }

  timer.GetPVRtimerinfo(timerinfo);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types)
{
  if (Timer::lifetimeValues == NULL)
    return PVR_ERROR_FAILED;

  //Note: schedule priority support is not implemented here
  //      The MediaPortal TV Server database has a priority field but their wiki
  //      says: "This feature is yet to be enabled".

  // One-shot epg-based (maps to MediaPortal 'Once')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::Once);
    type.SetAttributes(MPTV_RECORD_ONCE);
    type.SetDescription(kodi::addon::GetLocalizedString(30110)); /* Record once */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series weekly epg-based (maps to MediaPortal 'EveryTimeOnThisChannel')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::EveryTimeOnThisChannel);
    type.SetAttributes(MPTV_RECORD_EVERY_TIME_ON_THIS_CHANNEL);
    type.SetDescription(kodi::addon::GetLocalizedString(30115)); /* Record every time on this channel */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series weekly epg-based (maps to MediaPortal 'EveryTimeOnEveryChannel')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::EveryTimeOnEveryChannel);
    type.SetAttributes(MPTV_RECORD_EVERY_TIME_ON_EVERY_CHANNEL);
    type.SetDescription(kodi::addon::GetLocalizedString(30116)); /* Record every time on every channel */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series weekly epg-based (maps to MediaPortal 'Weekly')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::Weekly);
    type.SetAttributes(MPTV_RECORD_WEEKLY);
    type.SetDescription(kodi::addon::GetLocalizedString(30117)); /* "Record every week at this time" */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series daily epg-based (maps to MediaPortal 'Daily')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::Daily);
    type.SetAttributes(MPTV_RECORD_DAILY);
    type.SetDescription(kodi::addon::GetLocalizedString(30118)); /* Record every day at this time */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series Weekends epg-based (maps to MediaPortal 'WorkingDays')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::WorkingDays);
    type.SetAttributes(MPTV_RECORD_WORKING_DAYS);
    type.SetDescription(kodi::addon::GetLocalizedString(30114)); /* Record weekdays */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series Weekends epg-based (maps to MediaPortal 'Weekends')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::Weekends);
    type.SetAttributes(MPTV_RECORD_WEEEKENDS);
    type.SetDescription(kodi::addon::GetLocalizedString(30113)); /* Record Weekends */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  // Series weekly epg-based (maps to MediaPortal 'WeeklyEveryTimeOnThisChannel')
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::WeeklyEveryTimeOnThisChannel);
    type.SetAttributes(MPTV_RECORD_WEEKLY_EVERY_TIME_ON_THIS_CHANNEL);
    type.SetDescription(kodi::addon::GetLocalizedString(30119)); /* Weekly on this channel */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  /* Kodi specific 'Manual' schedule type */
  {
    kodi::addon::PVRTimerType type;
    type.SetId(cKodiTimerTypeOffset + TvDatabase::KodiManual);
    type.SetAttributes(MPTV_RECORD_MANUAL);
    type.SetDescription(kodi::addon::GetLocalizedString(30122)); /* Manual */
    Timer::lifetimeValues->SetLifeTimeValues(type);
    types.emplace_back(type);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::AddTimer(const kodi::addon::PVRTimer& timerinfo)
{
  string         result;

#ifdef _TIME32_T_DEFINED
  kodi::Log(ADDON_LOG_DEBUG, "->AddTimer Channel: %i, starttime: %i endtime: %i program: %s", timerinfo.GetClientChannelUid(), timerinfo.GetStartTime(), timerinfo.GetEndTime(), timerinfo.GetTitle().c_str());
#else
  kodi::Log(ADDON_LOG_DEBUG, "->AddTimer Channel: %i, 64 bit times not yet supported!", timerinfo.GetClientChannelUid());
#endif

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer(timerinfo);

  if (CSettings::Get().GetEnableOldSeriesDlg() && (timerinfo.GetStartTime() > 0) &&
      (timerinfo.GetEPGUid() != PVR_TIMER_NO_EPG_UID) &&
      ((timerinfo.GetTimerType() - cKodiTimerTypeOffset) == (unsigned int) TvDatabase::Once)
      )
  {
    /* New scheduled recording, not an instant or manual recording
     * Present a custom dialog with advanced recording settings
     */
    std::string strChannelName;
    if (timerinfo.GetClientChannelUid() >= 0)
    {
      strChannelName = m_channels[timerinfo.GetClientChannelUid()].Name();
    }
    CGUIDialogRecordSettings dlgRecSettings( timerinfo, timer, strChannelName);

    int dlogResult = dlgRecSettings.DoModal();

    if (dlogResult == 0)
      return PVR_ERROR_NO_ERROR;						// user canceled timer in dialog
  }

  result = SendCommand(timer.AddScheduleCommand());

  if(result.find("True") ==  string::npos)
  {
    kodi::Log(ADDON_LOG_DEBUG, "AddTimer for channel: %i [failed]", timerinfo.GetClientChannelUid());
    return PVR_ERROR_FAILED;
  }
  kodi::Log(ADDON_LOG_DEBUG, "AddTimer for channel: %i [done]", timerinfo.GetClientChannelUid());

  // Although Kodi adds this timer, we still have to trigger Kodi to update its timer list to
  // see this new timer at the Kodi side
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
  if (timerinfo.GetStartTime() <= 0)
  {
    // Refresh the recordings list to see the newly created recording
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::DeleteTimer(const kodi::addon::PVRTimer& timer, bool UNUSED(forceDelete))
{
  char           command[256];
  string         result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // Check if this timer has a parent schedule and a program id
  // When true, it has no real schedule at the Mediaportal side.
  // The best we can do in that case is disable the timer for this program only
  if ((timer.GetParentClientIndex() > 0) && (timer.GetEPGUid() > 0))
  {
    // Don't delete this timer, but disable it only
    kodi::addon::PVRTimer disableMe = timer;
    disableMe.SetState(PVR_TIMER_STATE_DISABLED);
    return UpdateTimer(disableMe);
  }

  cTimer mepotimer(timer);

  snprintf(command, 256, "DeleteSchedule:%i\n", mepotimer.Index());

  kodi::Log(ADDON_LOG_DEBUG, "DeleteTimer: About to delete MediaPortal schedule index=%i", mepotimer.Index());
  result = SendCommand(command);

  if(result.find("True") ==  string::npos)
  {
    kodi::Log(ADDON_LOG_DEBUG, "DeleteTimer %i [failed]", mepotimer.Index());
    return PVR_ERROR_FAILED;
  }
  kodi::Log(ADDON_LOG_DEBUG, "DeleteTimer %i [done]", mepotimer.Index());

  // Although Kodi deletes this timer, we still have to trigger Kodi to update its timer list to
  // remove the timer from the Kodi list
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::UpdateTimer(const kodi::addon::PVRTimer& timerinfo)
{
  string         result;

#ifdef _TIME32_T_DEFINED
  kodi::Log(ADDON_LOG_DEBUG, "->UpdateTimer Index: %i Channel: %i, starttime: %i endtime: %i program: %s", timerinfo.GetClientIndex(), timerinfo.GetClientChannelUid(), timerinfo.GetStartTime(), timerinfo.GetEndTime(), timerinfo.GetTitle());
#else
  kodi::Log(ADDON_LOG_DEBUG, "->UpdateTimer Channel: %i, 64 bit times not yet supported!", timerinfo.GetClientChannelUid());
#endif

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer(timerinfo);

  result = SendCommand(timer.UpdateScheduleCommand());
  if(result.find("True") ==  string::npos)
  {
    kodi::Log(ADDON_LOG_DEBUG, "UpdateTimer for channel: %i [failed]", timerinfo.GetClientChannelUid());
    return PVR_ERROR_FAILED;
  }
  kodi::Log(ADDON_LOG_DEBUG, "UpdateTimer for channel: %i [done]", timerinfo.GetClientChannelUid());

  // Although Kodi changes this timer, we still have to trigger Kodi to update its timer list to
  // see the timer changes at the Kodi side
  kodi::addon::CInstancePVRClient::TriggerTimerUpdate();

  return PVR_ERROR_NO_ERROR;
}


/************************************************************/
/** Live stream handling */

// The MediaPortal TV Server uses rtsp streams which Kodi can handle directly
// via the dvdplayer (ffmpeg) so we don't need to open the streams in this
// pvr addon.
// However, we still need to request the stream URL for the channel we want
// to watch as it is not known on beforehand.
// Most of the times it is the same URL for each selected channel. Only the
// stream itself changes. Example URL: rtsp://tvserverhost/stream2.0
// The number 2.0 may change when the tvserver is streaming multiple tv channels
// at the same time.
//
// The rtsp code from ffmpeg does not function well enough for this addon.
// Therefore the new TSReader version uses the Live555 library here to open rtsp
// urls or it can read directly from the timeshift buffer file.
bool cPVRClientMediaPortal::OpenLiveStream(const kodi::addon::PVRChannel& channelinfo)
{
  string result;
  char   command[256] = "";
  const char* sResolveRTSPHostname = booltostring(CSettings::Get().GetResolveRTSPHostname());
  vector<string> timeshiftfields;

  kodi::Log(ADDON_LOG_INFO, "Open Live stream for channel uid=%i", channelinfo.GetUniqueId());
  if (!IsUp())
  {
    m_iCurrentChannel = -1;
    m_bTimeShiftStarted = false;
    m_bSkipCloseLiveStream = false; //initialization
    m_signalStateCounter = 0;
    kodi::Log(ADDON_LOG_ERROR, "Open Live stream failed. No connection to backend.");
    return false;
  }

  if (((int)channelinfo.GetUniqueId()) == m_iCurrentChannel)
  {
    kodi::Log(ADDON_LOG_INFO, "New channel uid equal to the already streaming channel. Skipping re-tune.");
    return true;
  }

  m_iCurrentChannel = -1; // make sure that it is not a valid channel nr in case it will fail lateron
  m_signalStateCounter = 0;
  m_bTimeShiftStarted = false;
  m_bSkipCloseLiveStream = false; //initialization

  // Start the timeshift
  // Use the optimized TimeshiftChannel call (don't stop a running timeshift)
  snprintf(command, 256, "TimeshiftChannel:%i|%s|False\n", channelinfo.GetUniqueId(), sResolveRTSPHostname);
  result = SendCommand(command);

  if (result.find("ERROR") != std::string::npos || result.length() == 0)
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not start the timeshift for channel uid=%i. Reason: %s", channelinfo.GetUniqueId(), result.c_str());
    if (g_iTVServerKodiBuild>=109)
    {
      Tokenize(result, timeshiftfields, "|");
      //[0] = string error message
      //[1] = TvResult (optional field. SendCommand can also return a timeout)

      if(timeshiftfields.size()>1)
      {
        //For TVServer 1.2.1:
        //enum TvResult
        //{
        //  Succeeded = 0, (this is not an error)
        //  AllCardsBusy = 1,
        //  ChannelIsScrambled = 2,
        //  NoVideoAudioDetected = 3,
        //  NoSignalDetected = 4,
        //  UnknownError = 5,
        //  UnableToStartGraph = 6,
        //  UnknownChannel = 7,
        //  NoTuningDetails = 8,
        //  ChannelNotMappedToAnyCard = 9,
        //  CardIsDisabled = 10,
        //  ConnectionToSlaveFailed = 11,
        //  NotTheOwner = 12,
        //  GraphBuildingFailed = 13,
        //  SWEncoderMissing = 14,
        //  NoFreeDiskSpace = 15,
        //  NoPmtFound = 16,
        //};

        int tvresult = std::atoi(timeshiftfields[1].c_str());
        // Display one of the localized error messages 30060-30075
        kodi::QueueNotification(QUEUE_ERROR, "", kodi::addon::GetLocalizedString(30059 + tvresult));
      }
      else
      {
         kodi::QueueNotification(QUEUE_ERROR, "", result);
      }
    }
    else
    {
      if (result.find("[ERROR]: TVServer answer: ") != std::string::npos)
      {
        //Skip first part: "[ERROR]: TVServer answer: "
        kodi::QueueFormattedNotification(QUEUE_ERROR, "TVServer: %s", result.substr(26).c_str());
      }
      else
      {
        //Skip first part: "[ERROR]: "
        kodi::QueueNotification(QUEUE_ERROR, "", result.substr(7));
      }
    }
    m_iCurrentChannel = -1;
    if (m_tsreader != nullptr)
    {
      SafeDelete(m_tsreader);
    }
    return false;
  }
  else
  {
    Tokenize(result, timeshiftfields, "|");

    if(timeshiftfields.size()<4)
    {
      kodi::Log(ADDON_LOG_ERROR, "OpenLiveStream: Field count mismatch (<4). Data: %s\n", result.c_str());
      m_iCurrentChannel = -1;
      return false;
    }

    //[0] = rtsp url
    //[1] = original (unresolved) rtsp url
    //[2] = timeshift buffer filename
    //[3] = card id (TVServerKodi build >= 106)
    //[4] = tsbuffer pos (TVServerKodi build >= 110)
    //[5] = tsbuffer file nr (TVServerKodi build >= 110)

    m_PlaybackURL = timeshiftfields[0];
    if (CSettings::Get().GetStreamingMethod() == TSReader)
    {
      kodi::Log(ADDON_LOG_INFO, "Channel timeshift buffer: %s", timeshiftfields[2].c_str());
      if (channelinfo.GetIsRadio())
      {
        // 100 ms sleep to allow the buffer to fill
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    else
    {
      kodi::Log(ADDON_LOG_INFO, "Channel stream URL: %s", m_PlaybackURL.c_str());
    }

    if (CSettings::Get().GetSleepOnRTSPurl() > 0)
    {
      kodi::Log(ADDON_LOG_INFO, "Sleeping %i ms before opening stream: %s", CSettings::Get().GetSleepOnRTSPurl(), timeshiftfields[0].c_str());
      std::this_thread::sleep_for(std::chrono::milliseconds(CSettings::Get().GetSleepOnRTSPurl()));
    }

    // Check the returned stream URL. When the URL is an rtsp stream, we need
    // to close it again after watching to stop the timeshift.
    // A radio web stream (added to the TV Server) will return the web stream
    // URL without starting a timeshift.
    if(timeshiftfields[0].compare(0,4, "rtsp") == 0)
    {
      m_bTimeShiftStarted = true;
    }

    if (CSettings::Get().GetStreamingMethod() == TSReader)
    {
      if (m_tsreader != NULL)
      {
        bool bReturn = false;

        // Continue with the existing TsReader.
        kodi::Log(ADDON_LOG_INFO, "Re-using existing TsReader...");
        //if(g_bDirectTSFileRead)
        if(CSettings::Get().GetUseRTSP() == false)
        {
          m_tsreader->SetCardId(std::atoi(timeshiftfields[3].c_str()));

          if ((g_iTVServerKodiBuild >=110) && (timeshiftfields.size()>=6))
            bReturn = m_tsreader->OnZap(timeshiftfields[2].c_str(), atoll(timeshiftfields[4].c_str()), atol(timeshiftfields[5].c_str()));
          else
            bReturn = m_tsreader->OnZap(timeshiftfields[2].c_str(), -1, -1);
        }
        else
        {
          // RTSP url
          kodi::Log(ADDON_LOG_INFO, "Skipping OnZap for TSReader RTSP");
          bReturn = true; //Fast forward seek (OnZap) does not work for RTSP
        }

        if (bReturn)
        {
          m_iCurrentChannel = (int) channelinfo.GetUniqueId();
          m_iCurrentCard = std::atoi(timeshiftfields[3].c_str());
          m_bCurrentChannelIsRadio = channelinfo.GetIsRadio();
        }
        else
        {
          kodi::Log(ADDON_LOG_ERROR, "Re-using the existing TsReader failed.");
          CloseLiveStream();
        }

        return bReturn;
      }
      else
      {
        kodi::Log(ADDON_LOG_INFO, "Creating a new TsReader...");
        m_tsreader = new CTsReader();
      }

      if (!CSettings::Get().GetUseRTSP())
      {
        // Reading directly from the Timeshift buffer
        m_tsreader->SetCardSettings(&m_cCards);
        m_tsreader->SetCardId(std::atoi(timeshiftfields[3].c_str()));

        //if (g_szTimeshiftDir.length() > 0)
        //  m_tsreader->SetDirectory(g_szTimeshiftDir);

        if ( m_tsreader->Open(timeshiftfields[2].c_str()) != S_OK )
        {
          kodi::Log(ADDON_LOG_ERROR, "Cannot open timeshift buffer %s", timeshiftfields[2].c_str());
          CloseLiveStream();
          return false;
        }
      }
      else
      {
        // use the RTSP url and live555
        if ( m_tsreader->Open(timeshiftfields[0].c_str()) != S_OK)
        {
          kodi::Log(ADDON_LOG_ERROR, "Cannot open channel url %s", timeshiftfields[0].c_str());
          CloseLiveStream();
          return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
      }
    }

    // at this point everything is ready for playback
    m_iCurrentChannel = (int) channelinfo.GetUniqueId();
    m_iCurrentCard = std::atoi(timeshiftfields[3].c_str());
    m_bCurrentChannelIsRadio = channelinfo.GetIsRadio();
  }
  kodi::Log(ADDON_LOG_INFO, "OpenLiveStream: success for channel id %i (%s) on card %i", m_iCurrentChannel, channelinfo.GetChannelName().c_str(), m_iCurrentCard);

  return true;
}

int cPVRClientMediaPortal::ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  size_t read_wanted = iBufferSize;
  size_t read_done   = 0;
  static int read_timeouts  = 0;
  unsigned char* bufptr = pBuffer;

  //kodi::Log(ADDON_LOG_DEBUG, "->ReadLiveStream(buf_size=%i)", buf_size);
  if (CSettings::Get().GetStreamingMethod() != TSReader)
  {
    kodi::Log(ADDON_LOG_ERROR, "ReadLiveStream: this function should not be called in FFMPEG/RTSP mode. Use 'Reset the PVR database' to re-read the channel list");
    return 0;
  }

  if (!m_tsreader)
  {
    kodi::Log(ADDON_LOG_ERROR, "ReadLiveStream: failed. No open TSReader");
    return -1;
  }

  if ((m_tsreader->State() == State_Paused) && (CSettings::Get().GetUseRTSP()))
  {
    //kodi::Log(ADDON_LOG_ERROR, "ReadLiveStream: failed. Stream is paused");
    return 0;
  }

  while (read_done < static_cast<size_t>(iBufferSize))
  {
    read_wanted = iBufferSize - read_done;

    if (m_tsreader->Read(bufptr, read_wanted, &read_wanted) > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      read_timeouts++;
      return static_cast<int>(read_wanted);
    }
    read_done += read_wanted;

    if ( read_done < static_cast<size_t>(iBufferSize) )
    {
      if (read_timeouts > 200)
      {
        if (m_bCurrentChannelIsRadio == false || read_done == 0)
        {
          kodi::Log(ADDON_LOG_INFO, "Kodi requested %u bytes, but the TSReader got only %lu bytes in 2 seconds", iBufferSize, read_done);
        }
        read_timeouts = 0;

        //TODO
        //if read_done == 0 then check if the backend is still timeshifting,
        //or retrieve the reason why the timeshifting was stopped/failed...

        return static_cast<int>(read_done);
      }
      bufptr += read_wanted;
      read_timeouts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  read_timeouts = 0;

  return static_cast<int>(read_done);
}

void cPVRClientMediaPortal::CloseLiveStream(void)
{
  string result;

  if (!IsUp())
    return;

  if (m_bTimeShiftStarted && !m_bSkipCloseLiveStream)
  {
    if (CSettings::Get().GetStreamingMethod() == TSReader && m_tsreader)
    {
      m_tsreader->Close();
      SafeDelete(m_tsreader);
    }
    result = SendCommand("StopTimeshift:\n");
    kodi::Log(ADDON_LOG_INFO, "CloseLiveStream: %s", result.c_str());
    m_bTimeShiftStarted = false;
    m_iCurrentChannel = -1;
    m_iCurrentCard = -1;
    m_PlaybackURL.clear();

    m_signalStateCounter = 0;
  }
}

int64_t cPVRClientMediaPortal::SeekLiveStream(int64_t iPosition, int iWhence)
{
  if (CSettings::Get().GetStreamingMethod() == ffmpeg || !m_tsreader)
  {
    kodi::Log(ADDON_LOG_ERROR, "SeekLiveStream: is not supported in FFMPEG/RTSP mode.");
    return -1;
  }

  if (iPosition == 0 && iWhence == SEEK_CUR)
  {
    return m_tsreader->GetFilePointer();
  }
  return m_tsreader->SetFilePointer(iPosition, iWhence);
}

int64_t cPVRClientMediaPortal::LengthLiveStream(void)
{
  if (CSettings::Get().GetStreamingMethod() == ffmpeg || !m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

bool cPVRClientMediaPortal::IsRealTimeStream()
{
  return m_bTimeShiftStarted;
}

PVR_ERROR cPVRClientMediaPortal::GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus)
{
  if (g_iTVServerKodiBuild < 108 || (m_iCurrentChannel == -1))
  {
    // Not yet supported or playing webstream
    return PVR_ERROR_NO_ERROR;
  }

  string          result;

  // Limit the GetSignalQuality calls to once every 10 s
  if (m_signalStateCounter == 0)
  {
    // Request the signal quality for the current streaming card from the backend
    result = SendCommand("GetSignalQuality\n");

    if (result.length() > 0)
    {
      int signallevel = 0;
      int signalquality = 0;

      // Fetch the signal level and SNR values from the result string
      if (sscanf(result.c_str(),"%5i|%5i", &signallevel, &signalquality) == 2)
      {
        m_iSignal = (int) (signallevel * 655.35); // 100% is 0xFFFF 65535
        m_iSNR = (int) (signalquality * 655.35); // 100% is 0xFFFF 65535
      }
    }
  }
  m_signalStateCounter++;
  if (m_signalStateCounter > 10)
    m_signalStateCounter = 0;

  signalStatus.SetSignal(m_iSignal);
  signalStatus.SetSNR(m_iSNR);
  signalStatus.SetBER(m_signalStateCounter);
  signalStatus.SetAdapterStatus("timeshifting"); // hardcoded for now...


  if (m_iCurrentCard >= 0)
  {
    // Try to determine the name of the tv/radio card from the local card cache
    Card currentCard;
    if (m_cCards.GetCard(m_iCurrentCard, currentCard) == true)
    {
      signalStatus.SetAdapterName(currentCard.Name);
      return PVR_ERROR_NO_ERROR;
    }
  }

  signalStatus.SetAdapterName("");

  return PVR_ERROR_NO_ERROR;
}


/************************************************************/
/** Record stream handling */
// MediaPortal recordings are also rtsp streams. Main difference here with
// respect to the live tv streams is that the URLs for the recordings
// can be requested on beforehand (done in the TVServerKodi plugin).

bool cPVRClientMediaPortal::OpenRecordedStream(const kodi::addon::PVRRecording& recording, int64_t& streamId)
{
  kodi::Log(ADDON_LOG_INFO, "OpenRecordedStream (id=%s, RTSP=%d)", recording.GetRecordingId().c_str(), (CSettings::Get().GetUseRTSP() ? "true" : "false"));

  m_bTimeShiftStarted = false;

  if (!IsUp())
    return false;

  if (CSettings::Get().GetStreamingMethod() == ffmpeg)
  {
    kodi::Log(ADDON_LOG_ERROR, "Addon is in 'ffmpeg' mode. Kodi should play the RTSP url directly. Please reset your Kodi PVR database!");
    return false;
  }

  std::string recfile = "";

  cRecording* myrecording = GetRecordingInfo(recording);

  if (!myrecording)
  {
    return false;
  }

  if (!CSettings::Get().GetUseRTSP())
  {
    recfile  = myrecording->FilePath();
    if (recfile.empty())
    {
      kodi::Log(ADDON_LOG_ERROR, "Backend returned an empty recording filename for recording id %s.", recording.GetRecordingId().c_str());
      recfile = myrecording->Stream();
      if (!recfile.empty())
      {
        kodi::Log(ADDON_LOG_INFO, "Trying to use the recording RTSP stream URL name instead.");
      }
    }
  }
  else
  {
    recfile = myrecording->Stream();
    if (recfile.empty())
    {
      kodi::Log(ADDON_LOG_ERROR, "Backend returned an empty RTSP stream URL for recording id %s.", recording.GetRecordingId().c_str());
      recfile = myrecording->FilePath();
      if (!recfile.empty())
      {
        kodi::Log(ADDON_LOG_INFO, "Trying to use the filename instead.");
      }
    }
  }

  if (recfile.empty())
  {
    kodi::Log(ADDON_LOG_ERROR, "Recording playback not possible. Backend returned an empty filename and no RTSP stream URL for recording id %s", recording.GetRecordingId().c_str());
    kodi::QueueNotification(QUEUE_ERROR, "", kodi::addon::GetLocalizedString(30052));
    // Tell Kodi to re-read the list with recordings to remove deleted/non-existing recordings as a result of backend auto-deletion.
    kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
    return false;
  }

  // We have a recording file name or RTSP url, time to open it...
  m_tsreader = new CTsReader();
  m_tsreader->SetCardSettings(&m_cCards);
  if ( m_tsreader->Open(recfile.c_str()) != S_OK )
    return false;

  return true;
}

void cPVRClientMediaPortal::CloseRecordedStream(int64_t streamId)
{
  if (!IsUp() || CSettings::Get().GetStreamingMethod() == ffmpeg)
     return;

  if (m_tsreader)
  {
    kodi::Log(ADDON_LOG_INFO, "CloseRecordedStream: Stop TSReader...");
    m_tsreader->Close();
    SafeDelete(m_tsreader);
  }
  else
  {
    kodi::Log(ADDON_LOG_DEBUG, "CloseRecordedStream: Nothing to do.");
  }
}

int cPVRClientMediaPortal::ReadRecordedStream(int64_t streamId, unsigned char *pBuffer, unsigned int iBufferSize)
{
  size_t read_wanted = static_cast<size_t>(iBufferSize);
  size_t read_done   = 0;
  unsigned char* bufptr = pBuffer;

  if (CSettings::Get().GetStreamingMethod() == ffmpeg)
    return -1;

  while (read_done < static_cast<size_t>(iBufferSize))
  {
    read_wanted = iBufferSize - read_done;
    if (!m_tsreader)
      return -1;

    if (m_tsreader->Read(bufptr, read_wanted, &read_wanted) > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      return static_cast<int>(read_wanted);
    }
    read_done += read_wanted;

    if ( read_done < static_cast<size_t>(iBufferSize) )
    {
      bufptr += read_wanted;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  return static_cast<int>(read_done);
}

int64_t cPVRClientMediaPortal::SeekRecordedStream(int64_t streamId, int64_t iPosition, int iWhence)
{
  if (CSettings::Get().GetStreamingMethod() == ffmpeg || !m_tsreader)
  {
    return -1;
  }
#ifdef _DEBUG
  kodi::Log(ADDON_LOG_DEBUG, "SeekRec: Current pos %lli", m_tsreader->GetFilePointer());
#endif
  kodi::Log(ADDON_LOG_DEBUG,"SeekRec: iWhence %i pos %lli", iWhence, iPosition);

  return m_tsreader->SetFilePointer(iPosition, iWhence);
}

int64_t  cPVRClientMediaPortal::LengthRecordedStream(int64_t streamId)
{
  if (CSettings::Get().GetStreamingMethod() == ffmpeg || !m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

PVR_ERROR cPVRClientMediaPortal::GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording,
                                                              std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  // GetRecordingStreamProperties is called before OpenRecordedStream
  // If we return a stream URL here, Kodi will use its internal player to open the stream and bypass the PVR addon

  cRecording* myrecording = GetRecordingInfo(recording);

  if (!myrecording)
    return PVR_ERROR_NO_ERROR;

  properties.emplace_back(PVR_STREAM_PROPERTY_MIMETYPE, "video/mp2t");

  if (CSettings::Get().GetStreamingMethod() == ffmpeg)
  {
    properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, myrecording->Stream());
  }
  else if (!CSettings::Get().GetUseRTSP())
  {
    if (myrecording->IsRecording() == false)
    {
#ifdef TARGET_WINDOWS_DESKTOP
      if (OS::CFile::Exists(myrecording->FilePath()))
      {
        std::string recordingUri(ToKodiPath(myrecording->FilePath()));

        // Direct file playback by Kodi (without involving the addon)
        properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, recordingUri.c_str());
      }
#endif
    }
    else
    {
      // Indicate that this is a real-time stream
      properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelStreamProperties(const kodi::addon::PVRChannel& channel,
                                                            PVR_SOURCE source,
                                                            std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  // Is this a webstream?
  try
  {
    cChannel& selectedChannel = m_channels.at(channel.GetUniqueId());

    if (selectedChannel.IsWebstream())
    {
      kodi::Log(ADDON_LOG_DEBUG, "GetChannelStreamProperties (webstream) for uid=%i is '%s'",
                channel.GetUniqueId(), selectedChannel.URL());
      properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, selectedChannel.URL());
      properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
      return PVR_ERROR_NO_ERROR;
    }
  }
  catch (const std::out_of_range& oor)
  {
    // channel not found in our plugin channel cache
  }

  if (CSettings::Get().GetStreamingMethod() == ffmpeg)
  {
    // GetChannelStreamProperties is called before OpenLiveStream by Kodi, so we should already open the stream here...
    // The actual call to OpenLiveStream will return immediately since we've already tuned the correct channel here.
    if (m_bTimeShiftStarted == true)
    {
      //CloseLiveStream();
    }
    if (OpenLiveStream(channel) == true)
    {
      if (!m_PlaybackURL.empty())
      {
        kodi::Log(ADDON_LOG_DEBUG, "GetChannelStreamProperties (ffmpeg) for uid=%i is '%s'", channel.GetUniqueId(),
                  m_PlaybackURL.c_str());
        properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, m_PlaybackURL);
        properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
        properties.emplace_back(PVR_STREAM_PROPERTY_MIMETYPE, "video/mp2t");
        return PVR_ERROR_NO_ERROR;
      }
    }
  }
  else if (CSettings::Get().GetStreamingMethod() == TSReader)
  {
    if ((m_bTimeShiftStarted == true) && (CSettings::Get().GetFastChannelSwitch() == true))
    {
      // This ignores the next CloseLiveStream call from Kodi to speedup channel switching
      m_bSkipCloseLiveStream = true;
    }
  }
  else
  {
    kodi::Log(ADDON_LOG_ERROR, "GetChannelStreamProperties for uid=%i returned no URL",
              channel.GetUniqueId());
  }

  return PVR_ERROR_NO_ERROR;
}

void cPVRClientMediaPortal::PauseStream(bool bPaused)
{
  if (m_tsreader)
    m_tsreader->Pause(bPaused);
}

bool cPVRClientMediaPortal::CanPauseStream()
{
  if (m_tsreader)
    return true;
  else
    return false;
}

bool cPVRClientMediaPortal::CanSeekStream()
{
  if (m_tsreader)
    return true;
  else
    return false;
}

void cPVRClientMediaPortal::LoadGenreTable()
{
  // Read the genre string to type/subtype translation file:
  if(CSettings::Get().GetReadGenre())
  {
    string sGenreFile = UserPath() + PATH_SEPARATOR_CHAR + "resources" + PATH_SEPARATOR_CHAR + "genre_translation.xml";

    if (!kodi::vfs::FileExists(sGenreFile, false))
    {
      sGenreFile = UserPath() + PATH_SEPARATOR_CHAR + "genre_translation.xml";
      if (!kodi::vfs::FileExists(sGenreFile, false))
      {
        sGenreFile = ClientPath() + PATH_SEPARATOR_CHAR + "resources" + PATH_SEPARATOR_CHAR + "genre_translation.xml";
      }
    }

    m_genretable = new CGenreTable(sGenreFile);
  }
}

void cPVRClientMediaPortal::LoadCardSettings()
{
  kodi::Log(ADDON_LOG_DEBUG, "Loading card settings");

  /* Retrieve card settings (needed for Live TV and recordings folders) */
  vector<string> lines;

  if ( SendCommand2("GetCardSettings\n", lines) )
  {
    m_cCards.ParseLines(lines);
  }
}

void cPVRClientMediaPortal::SetConnectionState(PVR_CONNECTION_STATE newState)
{
  if (newState != m_state)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Connection state changed to '%s'",
      GetConnectionStateString(newState));
    m_state = newState;

    /* Notify connection state change (callback!) */
    std::string connection;
    GetConnectionString(connection);
    kodi::addon::CInstancePVRClient::ConnectionStateChange(connection, m_state, "");
  }
}

const char* cPVRClientMediaPortal::GetConnectionStateString(PVR_CONNECTION_STATE state) const
{
  switch (state)
  {
  case PVR_CONNECTION_STATE_SERVER_UNREACHABLE:
    return "Backend server is not reachable";
  case PVR_CONNECTION_STATE_SERVER_MISMATCH:
    return "Backend server is reachable, but the expected type of server is not running";
  case PVR_CONNECTION_STATE_VERSION_MISMATCH:
    return "Backend server is reachable, but the server version does not match client requirements";
  case PVR_CONNECTION_STATE_ACCESS_DENIED:
    return "Backend server is reachable, but denies client access (e.g. due to wrong credentials)";
  case PVR_CONNECTION_STATE_CONNECTED:
    return "Connection to backend server is established";
  case PVR_CONNECTION_STATE_DISCONNECTED:
    return "No connection to backend server (e.g. due to network errors or client initiated disconnect)";
  case PVR_CONNECTION_STATE_CONNECTING:
    return "Connecting to backend";
  case PVR_CONNECTION_STATE_UNKNOWN:
  default:
    return "Unknown state";
  }
}

cRecording* cPVRClientMediaPortal::GetRecordingInfo(const kodi::addon::PVRRecording& recording)
{
  // Is this the same recording as the previous one?
  if (m_lastSelectedRecording)
  {
    int recId = std::atoi(recording.GetRecordingId().c_str());
    if (m_lastSelectedRecording->Index() == recId)
    {
      return m_lastSelectedRecording;
    }
    SafeDelete(m_lastSelectedRecording);
  }

  if (!IsUp())
    return nullptr;

  string result;
  string command;

  command = StringUtils::Format("GetRecordingInfo:%s|%s|True|%s\n",
    recording.GetRecordingId().c_str(),
    ((CSettings::Get().GetUseRTSP() || CSettings::Get().GetStreamingMethod() == ffmpeg) ? "True" : "False"),
    CSettings::Get().GetResolveRTSPHostname() ? "True" : "False"
  );
  result = SendCommand(command);
  uri::decode(result);

  if (result.empty())
  {
    kodi::Log(ADDON_LOG_ERROR, "Backend command '%s' returned a zero-length answer.", command.c_str());
    return nullptr;
  }

  m_lastSelectedRecording = new cRecording();
  if (!m_lastSelectedRecording->ParseLine(result))
  {
    kodi::Log(ADDON_LOG_ERROR, "Parsing result from '%s' command failed. Result='%s'.", command.c_str(), result.c_str());
    return nullptr;
  }
  kodi::Log(ADDON_LOG_INFO, "RECORDING: %s", result.c_str());
  return m_lastSelectedRecording;
}

PVR_ERROR cPVRClientMediaPortal::GetStreamTimes(kodi::addon::PVRStreamTimes& stream_times)
{
  if (!m_bTimeShiftStarted && m_lastSelectedRecording)
  {
    // Recording playback
    // Warning: documentation in xbmc_pvr_types.h is wrong. pts values are not in seconds.
    stream_times.SetStartTime(0); // seconds
    stream_times.SetPTSStart(0);  // Unit must match Kodi's internal m_clock.GetClock() which is in useconds
    stream_times.SetPTSBegin(0);  // useconds
    stream_times.SetPTSEnd(((int64_t)m_lastSelectedRecording->Duration()) * STREAM_TIME_BASE); //useconds
    return PVR_ERROR_NO_ERROR;
  }
  else if (m_bTimeShiftStarted)
  {
    stream_times.SetStartTime(m_tsreader->GetStartTime());
    stream_times.SetPTSStart(0);  // Unit must match Kodi's internal m_clock.GetClock() which is in useconds
    stream_times.SetPTSBegin(m_tsreader->GetPtsBegin());  // useconds
    stream_times.SetPTSEnd(m_tsreader->GetPtsEnd());
    return PVR_ERROR_NO_ERROR;
  }
  *stream_times = { 0 };

  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR cPVRClientMediaPortal::GetStreamReadChunkSize(int& chunksize)
{
  chunksize = 32 * 1024;
  return PVR_ERROR_NO_ERROR;
}

namespace
{
    int GetFileContents(const std::string& url, std::string& content)
    {
      content.clear();
      kodi::vfs::CFile file;
      if (file.OpenFile(url))
      {
        char buffer[1024];
        while (int bytesRead = file.Read(buffer, 1024))
          content.append(buffer, bytesRead);
      }

      return content.length();
    }
}

PVR_ERROR cPVRClientMediaPortal::GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl) // bilkusg
{
    cRecording* myrecording = GetRecordingInfo(recording);
    if (!myrecording)
      return PVR_ERROR_SERVER_ERROR;

    const std::string pvrSmbPath = m_tsreader->TranslatePath(myrecording->FilePath());
    const std::string edlFilePath = pvrSmbPath.substr(0, pvrSmbPath.find_last_of('.')) + ".edl";
    kodi::Log(ADDON_LOG_DEBUG, "GetRecordingEdl for %s from %s", myrecording->FilePath(), edlFilePath.c_str());
    
    if (!kodi::vfs::FileExists(edlFilePath))
      return PVR_ERROR_NO_ERROR;
    std::string edlFileContents;
    int edlLength = GetFileContents(edlFilePath, edlFileContents);
    if (edlLength == 0) 
      return PVR_ERROR_NO_ERROR;

    std::istringstream stream(edlFileContents);
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line))
    {
        float start = 0.0f, stop = 0.0f;
        unsigned int type = PVR_EDL_TYPE_CUT;
        lineNumber++;
        if (std::sscanf(line.c_str(), "%f %f %u", &start, &stop, &type) < 2 || type > PVR_EDL_TYPE_COMBREAK)
        {
            kodi::Log(ADDON_LOG_INFO, "%s Unable to parse EDL entry for recording '%s' at line %d. Skipping.", __func__,
                edlFilePath.c_str(), lineNumber);
            continue;
        }

        start = std::max(start, 0.0f);
        stop = std::max(stop, 0.0f);
        start = std::min(start, stop);
        stop = std::max(start, stop);

        kodi::Log(ADDON_LOG_DEBUG, "%s EDL for '%s', line %d -  start: %f stop: %f type: %d", __func__, edlFilePath.c_str(), lineNumber, start, stop, type);

        kodi::addon::PVREDLEntry edlEntry;
        edlEntry.SetStart(static_cast<int64_t>(start * 1000.0f));
        edlEntry.SetEnd(static_cast<int64_t>(stop * 1000.0f));
        edlEntry.SetType(static_cast<PVR_EDL_TYPE>(type));

        edl.emplace_back(edlEntry);
    }
    kodi::Log(ADDON_LOG_INFO, "%s - recording '%s' has '%d' EDL entries available", __func__, recording.GetTitle().c_str(), edl.size());
    return PVR_ERROR_NO_ERROR;
}
