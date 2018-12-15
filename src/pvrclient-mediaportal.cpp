/*
 *      Copyright (C) 2005-2013 Team Kodi
 *      https://kodi.tv
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

#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <clocale>

#include "p8-platform/util/timeutils.h"
#include "p8-platform/util/StringUtils.h"

#include "client.h"
#include "timers.h"
#include "channels.h"
#include "recordings.h"
#include "epg.h"
#include "utils.h"
#include "pvrclient-mediaportal.h"
#include "lib/tsreader/TSReader.h"
#ifdef TARGET_WINDOWS
#include "FileUtils.h"
#endif
#include "GUIDialogRecordSettings.h"

using namespace std;
using namespace ADDON;
using namespace MPTV;

/* Globals */
int g_iTVServerKodiBuild = 0;

/* TVServerKodi plugin supported versions */
#define TVSERVERKODI_MIN_VERSION_STRING         "1.1.7.107"
#define TVSERVERKODI_MIN_VERSION_BUILD          107
#define TVSERVERKODI_RECOMMENDED_VERSION_STRING "1.2.3.122 till 1.20.0.140"
#define TVSERVERKODI_RECOMMENDED_VERSION_BUILD  140

/************************************************************/
/** Class interface */

cPVRClientMediaPortal::cPVRClientMediaPortal() :
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
  m_iLastRecordingUpdate   = 0;
  m_signalStateCounter     = 0;
  m_iSignal                = 0;
  m_iSNR                   = 0;
  m_lastSelectedRecording  = NULL;

  /* Generate the recording life time strings */
  Timer::lifetimeValues = new cLifeTimeValues();
}

cPVRClientMediaPortal::~cPVRClientMediaPortal()
{
  KODI->Log(LOG_DEBUG, "->~cPVRClientMediaPortal()");
  Disconnect();
  SAFE_DELETE(Timer::lifetimeValues);
  SAFE_DELETE(m_tcpclient);
  SAFE_DELETE(m_genretable);
  SAFE_DELETE(m_lastSelectedRecording);
}

string cPVRClientMediaPortal::SendCommand(const char* command)
{
  std::string cmd(command);
  return SendCommand(cmd);
}

string cPVRClientMediaPortal::SendCommand(const string& command)
{
  P8PLATFORM::CLockObject critsec(m_mutex);

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
          KODI->Log(LOG_ERROR, "SendCommand('%s') failed.", command.c_str());
          return "";
        }
      }
      else
      {
        KODI->Log(LOG_ERROR, "SendCommand: reconnect failed.");
        return "";
      }
    }
  }

  string result;

  if ( !m_tcpclient->ReadLine( result ) )
  {
    KODI->Log(LOG_ERROR, "SendCommand - Failed.");
    return "";
  }

  if (result.find("[ERROR]:") != std::string::npos)
  {
    KODI->Log(LOG_ERROR, "TVServerKodi error: %s", result.c_str());
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
  KODI->Log(LOG_INFO, "Mediaportal pvr addon " STR(MPTV_VERSION) " connecting to %s:%i", g_szHostname.c_str(), g_iPort);

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
      KODI->Log(LOG_ERROR, "Could not connect to MediaPortal TV Server backend.");
      // Start background thread for connecting to the backend
      if (!IsRunning())
      {
        KODI->Log(LOG_INFO, "Waiting for a connection in the background.");
        CreateThread();
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
  P8PLATFORM::CLockObject critsec(m_connectionMutex);

  string result;

  if (!m_tcpclient->create())
  {
    KODI->Log(LOG_ERROR, "Could not connect create socket");
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

  if (!m_tcpclient->connect(g_szHostname, (unsigned short) g_iPort))
  {
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_SERVER_UNREACHABLE);
    }
    return PVR_CONNECTION_STATE_SERVER_UNREACHABLE;
  }

  m_tcpclient->set_non_blocking(1);
  KODI->Log(LOG_INFO, "Connected to %s:%i", g_szHostname.c_str(), g_iPort);

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
    KODI->Log(LOG_ERROR, "TVServer does not accept protocol: PVRclientXBMC:0-1");
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
    KODI->Log(LOG_ERROR, "Your TVServerKodi version is too old. Please upgrade to '%s' or higher!", TVSERVERKODI_MIN_VERSION_STRING);
    KODI->QueueNotification(QUEUE_ERROR, KODI->GetLocalizedString(30051), TVSERVERKODI_MIN_VERSION_STRING);
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
    KODI->Log(LOG_ERROR, "Could not parse the TVServerKodi version string '%s'", fields[1].c_str());
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
    }
    return PVR_CONNECTION_STATE_VERSION_MISMATCH;
  }

  // Check for the minimal requirement: 1.1.0.70
  if( g_iTVServerKodiBuild < TVSERVERKODI_MIN_VERSION_BUILD ) //major < 1 || minor < 1 || revision < 0 || build < 70
  {
    KODI->Log(LOG_ERROR, "Your TVServerKodi version '%s' is too old. Please upgrade to '%s' or higher!", fields[1].c_str(), TVSERVERKODI_MIN_VERSION_STRING);
    KODI->QueueNotification(QUEUE_ERROR, KODI->GetLocalizedString(30050), fields[1].c_str(), TVSERVERKODI_MIN_VERSION_STRING);
    if (updateConnectionState)
    {
      SetConnectionState(PVR_CONNECTION_STATE_VERSION_MISMATCH);
    }
    return PVR_CONNECTION_STATE_VERSION_MISMATCH;
  }
  else
  {
    KODI->Log(LOG_INFO, "Your TVServerKodi version is '%s'", fields[1].c_str());
        
    // Advice to upgrade:
    if( g_iTVServerKodiBuild < TVSERVERKODI_RECOMMENDED_VERSION_BUILD )
    {
      KODI->Log(LOG_INFO, "It is adviced to upgrade your TVServerKodi version '%s' to '%s' or higher!", fields[1].c_str(), TVSERVERKODI_RECOMMENDED_VERSION_STRING);
    }
  }

  /* Store connection string */
  char buffer[512];
  snprintf(buffer, 512, "%s:%i", g_szHostname.c_str(), g_iPort);
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

  KODI->Log(LOG_INFO, "Disconnect");

  if (IsRunning())
  {
    StopThread(1000);
  }

  if (m_tcpclient->is_valid() && m_bTimeShiftStarted)
  {
    result = SendCommand("IsTimeshifting:\n");

    if (result.find("True") != std::string::npos )
    {
      if ((g_eStreamingMethod==TSReader) && (m_tsreader != NULL))
      {
        m_tsreader->Close();
        SAFE_DELETE(m_tsreader);
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

void* cPVRClientMediaPortal::Process(void)
{
  KODI->Log(LOG_DEBUG, "Background thread started.");

  bool keepWaiting = true;
  PVR_CONNECTION_STATE state;

  while (!IsStopped() && keepWaiting)
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
      usleep(60000000);
    }
  }
  SetConnectionState(state);

  KODI->Log(LOG_DEBUG, "Background thread finished.");

  return NULL;
}


/************************************************************/
/** General handling */

// Used among others for the server name string in the "Recordings" view
const char* cPVRClientMediaPortal::GetBackendName(void)
{
  if (!IsUp())
  {
    return g_szHostname.c_str();
  }

  KODI->Log(LOG_DEBUG, "->GetBackendName()");

  if (m_BackendName.length() == 0)
  {
    m_BackendName = "MediaPortal TV-server (";
    m_BackendName += SendCommand("GetBackendName:\n");
    m_BackendName += ")";
  }

  return m_BackendName.c_str();
}

const char* cPVRClientMediaPortal::GetBackendVersion(void)
{
  if (!IsUp())
    return "0.0";

  if(m_BackendVersion.length() == 0)
  {
    m_BackendVersion = SendCommand("GetVersion:\n");
  }

  KODI->Log(LOG_DEBUG, "GetBackendVersion: %s", m_BackendVersion.c_str());

  return m_BackendVersion.c_str();
}

const char* cPVRClientMediaPortal::GetConnectionString(void)
{
  if (m_ConnectionString.empty())
    return "";

  KODI->Log(LOG_DEBUG, "GetConnectionString: %s", m_ConnectionString.c_str());
  return m_ConnectionString.c_str();
}

PVR_ERROR cPVRClientMediaPortal::GetDriveSpace(long long *iTotal, long long *iUsed)
{
  string result;
  vector<string> fields;

  *iTotal = 0;
  *iUsed = 0;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetDriveSpace:\n");

  Tokenize(result, fields, "|");

  if(fields.size() >= 2)
  {
    *iTotal = (long long) atoi(fields[0].c_str());
    *iUsed = (long long) atoi(fields[1].c_str());
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
      KODI->Log(LOG_DEBUG, "GetMPTVTime: time from MP TV Server: %d-%d-%d %d:%d:%d, offset %d seconds", year, month, day, hour, minute, second, m_BackendUTCoffset );
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
        KODI->Log(LOG_DEBUG, "GetMPTVTime: Unable to convert string '%s' into date+time", fields[0].c_str());
        return PVR_ERROR_SERVER_ERROR;
      }

      KODI->Log(LOG_DEBUG, "GetMPTVTime: localtime %s", asctime(localtime(&m_BackendTime)));
      KODI->Log(LOG_DEBUG, "GetMPTVTime: gmtime    %s", asctime(gmtime(&m_BackendTime)));

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

PVR_ERROR cPVRClientMediaPortal::GetEpg(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  vector<string> lines;
  char           command[256];
  string         result;
  cEpg           epg;
  EPG_TAG        broadcast;
  struct tm      starttime;
  struct tm      endtime;

  starttime = *gmtime( &iStart );
  endtime = *gmtime( &iEnd );

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // Request (extended) EPG data for the given period
  snprintf(command, 256, "GetEPG:%i|%04d-%02d-%02dT%02d:%02d:%02d.0Z|%04d-%02d-%02dT%02d:%02d:%02d.0Z\n",
          channel.iUniqueId,                                                 //Channel id
          starttime.tm_year + 1900, starttime.tm_mon + 1, starttime.tm_mday, //Start date     [2..4]
          starttime.tm_hour, starttime.tm_min, starttime.tm_sec,             //Start time     [5..7]
          endtime.tm_year + 1900, endtime.tm_mon + 1, endtime.tm_mday,       //End date       [8..10]
          endtime.tm_hour, endtime.tm_min, endtime.tm_sec);                  //End time       [11..13]

  result = SendCommand(command);

  if(result.compare(0,5, "ERROR") != 0)
  {
    if( result.length() != 0)
    {
      memset(&broadcast, 0, sizeof(EPG_TAG));
      epg.SetGenreTable(m_genretable);

      Tokenize(result, lines, ",");

      KODI->Log(LOG_DEBUG, "Found %i EPG items for channel %i\n", lines.size(), channel.iUniqueId);

      for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
      {
        string& data(*it);

        if( data.length() > 0)
        {
          uri::decode(data);

          bool isEnd = epg.ParseLine(data);

          if (isEnd && epg.StartTime() != 0)
          {
            broadcast.iUniqueBroadcastId  = epg.UniqueId();
            broadcast.strTitle            = epg.Title();
            broadcast.iUniqueChannelId    = channel.iUniqueId;
            broadcast.startTime           = epg.StartTime();
            broadcast.endTime             = epg.EndTime();
            broadcast.strPlotOutline      = epg.PlotOutline();
            broadcast.strPlot             = epg.Description();
            broadcast.strIconPath         = "";
            broadcast.iGenreType          = epg.GenreType();
            broadcast.iGenreSubType       = epg.GenreSubType();
            broadcast.strGenreDescription = epg.Genre();
            broadcast.firstAired          = epg.OriginalAirDate();
            broadcast.iParentalRating     = epg.ParentalRating();
            broadcast.iStarRating         = epg.StarRating();
            broadcast.bNotify             = false;
            broadcast.iSeriesNumber       = epg.SeriesNumber();
            broadcast.iEpisodeNumber      = epg.EpisodeNumber();
            broadcast.iEpisodePartNumber  = atoi(epg.EpisodePart());
            broadcast.strEpisodeName      = epg.EpisodeName();
            broadcast.iFlags              = EPG_TAG_FLAG_UNDEFINED;

            PVR->TransferEpgEntry(handle, &broadcast);
          }
          epg.Reset();
        }
      }
    }
    else
    {
      KODI->Log(LOG_DEBUG, "No EPG items found for channel %i", channel.iUniqueId);
    }
  }
  else
  {
    KODI->Log(LOG_DEBUG, "RequestEPGForChannel(%i) %s", channel.iUniqueId, result.c_str());
  }

  return PVR_ERROR_NO_ERROR;
}


/************************************************************/
/** Channel handling */

int cPVRClientMediaPortal::GetNumChannels(void)
{
  string result;

  if (!IsUp())
    return -1;

  // Get the total channel count (radio+tv)
  // It is only used to check whether Kodi should request the channel list
  result = SendCommand("GetChannelCount:\n");

  return atol(result.c_str());
}

PVR_ERROR cPVRClientMediaPortal::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  vector<string>  lines;
  std::string     command;
  const char *    baseCommand;
  PVR_CHANNEL     tag;
  std::string     stream;
  std::string     groups;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(bRadio)
  {
    if(!g_bRadioEnabled)
    {
      KODI->Log(LOG_INFO, "Fetching radio channels is disabled.");
      return PVR_ERROR_NO_ERROR;
    }

    baseCommand = "ListRadioChannels";
    if (g_szRadioGroup.empty())
    {
      KODI->Log(LOG_DEBUG, "GetChannels(radio) all channels");
    }
    else
    {
      KODI->Log(LOG_DEBUG, "GetChannels(radio) for radio group(s): '%s'", g_szRadioGroup.c_str());
      groups = uri::encode(uri::PATH_TRAITS, g_szRadioGroup);
      StringUtils::Replace(groups, "%7C","|");
    }
  }
  else
  {
    baseCommand = "ListTVChannels";
    if (g_szTVGroup.empty())
    {
      KODI->Log(LOG_DEBUG, "GetChannels(tv) all channels");
    }
    else
    {
      KODI->Log(LOG_DEBUG, "GetChannels(tv) for TV group(s): '%s'", g_szTVGroup.c_str());
      groups = uri::encode(uri::PATH_TRAITS, g_szTVGroup);
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
    if (bRadio)
      strThumbPath += "Radio\\";
    else
      strThumbPath += "TV\\logos\\";

    bCheckForThumbs = OS::CFile::Exists(strThumbPath);
  }
#endif // TARGET_WINDOWS_DESKTOP

  memset(&tag, 0, sizeof(PVR_CHANNEL));

  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      if(bRadio)
        KODI->Log(LOG_DEBUG, "TVServer returned no data. Empty/non existing radio group '%s'?", g_szRadioGroup.c_str());
      else
        KODI->Log(LOG_DEBUG, "TVServer returned no data. Empty/non existing tv group '%s'?", g_szTVGroup.c_str());
      break;
    }

    uri::decode(data);

    cChannel channel;
    if( channel.Parse(data) )
    {
      // Cache this channel in our local uid-channel list
      // This cache is used for the GUIDialogRecordSettings
      m_channelNames[channel.UID()] = channel.Name();

      // Prepare the PVR_CHANNEL struct to transfer this channel to Kodi
      tag.iUniqueId = channel.UID();
      if (channel.MajorChannelNr() == -1)
      {
        tag.iChannelNumber = channel.ExternalID();
      }
      else
      {
        tag.iChannelNumber = channel.MajorChannelNr();
        tag.iSubChannelNumber = channel.MinorChannelNr();
      }
      PVR_STRCPY(tag.strChannelName, channel.Name());
      PVR_STRCLR(tag.strIconPath);
#ifdef TARGET_WINDOWS_DESKTOP
      if (bCheckForThumbs)
      {
        const int ciExtCount = 5;
        string strIconExt [ciExtCount] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };
        string strIconName;
        string strIconBaseName;

        KODI->Log(LOG_DEBUG, "Checking for a channel thumbnail for channel %s in %s", channel.Name(), strThumbPath.c_str());

        strIconBaseName = strThumbPath + ToThumbFileName(channel.Name());

        for (int i=0; i < ciExtCount; i++)
        {
          strIconName = strIconBaseName + strIconExt[i];
          if ( OS::CFile::Exists(strIconName) )
          {
            PVR_STRCPY(tag.strIconPath, strIconName.c_str());
            KODI->Log(LOG_DEBUG, "Found channel thumb: %s", tag.strIconPath);
            break;
          }
        }
      }
#endif
      tag.iEncryptionSystem = channel.Encrypted();
      tag.bIsRadio = bRadio;
      tag.bIsHidden = !channel.VisibleInGuide();

      if(channel.IsWebstream())
      {
        KODI->Log(LOG_DEBUG, "Channel '%s' has a webstream: %s. TODO fixme.", channel.Name(), channel.URL());
        PVR_STRCLR(tag.strInputFormat);
      }
      else
      {
        if (g_eStreamingMethod==TSReader)
        {
          // TSReader
          //Use OpenLiveStream to read from the timeshift .ts file or an rtsp stream
          if (!bRadio)
            PVR_STRCPY(tag.strInputFormat, "video/mp2t");
          else
            PVR_STRCLR(tag.strInputFormat);
        }
        else
        {
          //Use GetLiveStreamURL to fetch an rtsp stream
          KODI->Log(LOG_DEBUG, "Channel '%s' has a rtsp stream: %s. TODO fixme.", channel.Name(), channel.URL());
          PVR_STRCLR(tag.strInputFormat);
        }
      }

      if( (!g_bOnlyFTA) || (tag.iEncryptionSystem==0))
      {
        PVR->TransferChannelEntry(handle, &tag);
      }
    }
  }

  //pthread_mutex_unlock(&m_critSection);
  return PVR_ERROR_NO_ERROR;
}

/************************************************************/
/** Channel group handling **/

int cPVRClientMediaPortal::GetChannelGroupsAmount(void)
{
  // Not directly possible at the moment
  KODI->Log(LOG_DEBUG, "GetChannelGroupsAmount: TODO");

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // just tell Kodi that we have groups
  return 1;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  vector<string>  lines;
  std::string   filters;
  PVR_CHANNEL_GROUP tag;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(bRadio)
  {
    if (!g_bRadioEnabled)
    {
      KODI->Log(LOG_DEBUG, "Skipping GetChannelGroups for radio. Radio support is disabled.");
      return PVR_ERROR_NO_ERROR;
    }

    filters = g_szRadioGroup;

    KODI->Log(LOG_DEBUG, "GetChannelGroups for radio");
    if (!SendCommand2("ListRadioGroups\n", lines))
      return PVR_ERROR_SERVER_ERROR;
  }
  else
  {
    filters = g_szTVGroup;

    KODI->Log(LOG_DEBUG, "GetChannelGroups for TV");
    if (!SendCommand2("ListGroups\n", lines))
      return PVR_ERROR_SERVER_ERROR;
  }

  memset(&tag, 0, sizeof(PVR_CHANNEL_GROUP));

  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      KODI->Log(LOG_DEBUG, "TVServer returned no data. No %s groups found?", ((bRadio) ? "radio" : "tv"));
      break;
    }

    uri::decode(data);

    if (data.compare("All Channels") == 0)
    {
      KODI->Log(LOG_DEBUG, "Skipping All Channels (%s) group", ((bRadio) ? "radio" : "tv"), tag.strGroupName);
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

      tag.bIsRadio = bRadio;
      PVR_STRCPY(tag.strGroupName, data.c_str());
      KODI->Log(LOG_DEBUG, "Adding %s group: %s", ((bRadio) ? "radio" : "tv"), tag.strGroupName);
      PVR->TransferChannelGroup(handle, &tag);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  //TODO: code below is similar to GetChannels code. Refactor and combine...
  vector<string>           lines;
  std::string              command;
  PVR_CHANNEL_GROUP_MEMBER tag;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if (group.bIsRadio)
  {
    if (g_bRadioEnabled)
    {
      KODI->Log(LOG_DEBUG, "GetChannelGroupMembers: for radio group '%s'", group.strGroupName);
      command = StringUtils::Format("ListRadioChannels:%s\n", uri::encode(uri::PATH_TRAITS, group.strGroupName).c_str());
    }
    else
    {
      KODI->Log(LOG_DEBUG, "Skipping GetChannelGroupMembers for radio. Radio support is disabled.");
      return PVR_ERROR_NO_ERROR;
    }
  }
  else
  {
    KODI->Log(LOG_DEBUG, "GetChannelGroupMembers: for tv group '%s'", group.strGroupName);
    command = StringUtils::Format("ListTVChannels:%s\n", uri::encode(uri::PATH_TRAITS, group.strGroupName).c_str());
  }

  if (!SendCommand2(command, lines))
    return PVR_ERROR_SERVER_ERROR;

  memset(&tag, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

  for (vector<string>::iterator it = lines.begin(); it < lines.end(); ++it)
  {
    string& data(*it);

    if (data.length() == 0)
    {
      if(group.bIsRadio)
        KODI->Log(LOG_DEBUG, "TVServer returned no data. Empty/non existing radio group '%s'?", g_szRadioGroup.c_str());
      else
        KODI->Log(LOG_DEBUG, "TVServer returned no data. Empty/non existing tv group '%s'?", g_szTVGroup.c_str());
      break;
    }

    uri::decode(data);

    cChannel channel;
    if( channel.Parse(data) )
    {
      tag.iChannelUniqueId = channel.UID();
      if (channel.MajorChannelNr() == -1)
      {
        tag.iChannelNumber = channel.ExternalID();
      }
      else
      {
        tag.iChannelNumber = channel.MajorChannelNr();
        tag.iSubChannelNumber = channel.MinorChannelNr();
      }
      PVR_STRCPY(tag.strGroupName, group.strGroupName);


      // Don't add encrypted channels when FTA only option is turned on
      if( (!g_bOnlyFTA) || (channel.Encrypted()==false))
      {
        KODI->Log(LOG_DEBUG, "GetChannelGroupMembers: add channel %s to group '%s' (Backend channel uid=%d, channelnr=%d)",
          channel.Name(), group.strGroupName, tag.iChannelUniqueId, tag.iChannelNumber);
        PVR->TransferChannelGroupMember(handle, &tag);
      }
    }
  }

  return PVR_ERROR_NO_ERROR;
}

/************************************************************/
/** Record handling **/

int cPVRClientMediaPortal::GetNumRecordings(void)
{
  string            result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetRecordingCount:\n");

  return atol(result.c_str());
}

PVR_ERROR cPVRClientMediaPortal::GetRecordings(ADDON_HANDLE handle)
{
  vector<string>  lines;
  string          result;
  PVR_RECORDING   tag;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  if(g_bResolveRTSPHostname == false)
  {
    result = SendCommand("ListRecordings:False\n");
  }
  else
  {
    result = SendCommand("ListRecordings\n");
  }

  if( result.length() == 0 )
  {
    KODI->Log(LOG_DEBUG, "Backend returned no recordings" );
    return PVR_ERROR_NO_ERROR;
  }

  Tokenize(result, lines, ",");

  memset(&tag, 0, sizeof(PVR_RECORDING));

  for (vector<string>::iterator it = lines.begin(); it != lines.end(); ++it)
  {
    string& data(*it);
    uri::decode(data);

    KODI->Log(LOG_DEBUG, "RECORDING: %s", data.c_str() );

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

      PVR_STRCPY(tag.strRecordingId, strRecordingId.c_str());
      PVR_STRCPY(tag.strTitle, recording.Title());
      PVR_STRCPY(tag.strEpisodeName, recording.EpisodeName());
      PVR_STRCPY(tag.strPlot, recording.Description());
      PVR_STRCPY(tag.strChannelName, recording.ChannelName());
      tag.iChannelUid    = recording.ChannelID();
      tag.recordingTime  = recording.StartTime();
      tag.iDuration      = (int) recording.Duration();
      tag.iPriority      = 0; // only available for schedules, not for recordings
      tag.iLifetime      = recording.Lifetime();
      tag.iGenreType     = recording.GenreType();
      tag.iGenreSubType  = recording.GenreSubType();
      PVR_STRCPY(tag.strGenreDescription, recording.GetGenre());
      tag.iPlayCount     = recording.TimesWatched();
      tag.iLastPlayedPosition = recording.LastPlayedPosition();
      tag.iEpisodeNumber = recording.GetEpisodeNumber();
      tag.iSeriesNumber  = recording.GetSeriesNumber();
      tag.iEpgEventId    = EPG_TAG_INVALID_UID;
      tag.channelType    = recording.GetChannelType();

      strDirectory = recording.Directory();
      if (strDirectory.length() > 0)
      {
        StringUtils::Replace(strDirectory, "\\", " - "); // Kodi supports only 1 sublevel below Recordings, so flatten the MediaPortal directory structure
        PVR_STRCPY(tag.strDirectory, strDirectory.c_str()); // used in Kodi as directory structure below "Recordings"
        if ((StringUtils::EqualsNoCase(strDirectory, tag.strTitle)) && (strEpisodeName.length() > 0))
        {
          strEpisodeName = recording.Title();
          strEpisodeName+= " - ";
          strEpisodeName+= recording.EpisodeName();
          PVR_STRCPY(tag.strTitle, strEpisodeName.c_str());
        }
      }
      else
      {
        PVR_STRCLR(tag.strDirectory);
      }

      PVR_STRCLR(tag.strThumbnailPath);

#ifdef TARGET_WINDOWS_DESKTOP
      std::string recordingUri(ToKodiPath(recording.FilePath()));
      if (g_bUseRTSP == false)
      {
        /* Recording thumbnail */
        std::string strThumbnailName(recordingUri);
        StringUtils::Replace(strThumbnailName, ".ts", ".jpg");
        /* Check if it exists next to the recording */
        if (KODI->FileExists(strThumbnailName.c_str(), false))
        {
          PVR_STRCPY(tag.strThumbnailPath, strThumbnailName.c_str());
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
            if (KODI->FileExists(strThumbnailName.c_str(), false))
            {
              PVR_STRCPY(tag.strThumbnailPath, strThumbnailName.c_str());
            }
            else
            {
              /* MediaPortal 2 */
              strThumbnailName = strProgramData +
                                 "\\Team MediaPortal\\MP2-Server\\SlimTVCore\\v3.0\\thumbs\\" +
                                 strThumbnailFilename;
              if (KODI->FileExists(strThumbnailName.c_str(), false))
              {
                PVR_STRCPY(tag.strThumbnailPath, strThumbnailName.c_str());
              }
            }
          }
          else
            PVR_STRCLR(tag.strThumbnailPath);
        }
      }
#endif /* TARGET_WINDOWS_DESKTOP */

      if (g_eStreamingMethod!=TSReader)
      {
        // Use rtsp url and Kodi's internal FFMPeg playback
        KODI->Log(LOG_DEBUG, "Recording '%s' has a rtsp url '%s'. TODO Fix me. ", recording.Title(), recording.Stream());
      }

      PVR->TransferRecordingEntry(handle, &tag);
    }
  }

  m_iLastRecordingUpdate = P8PLATFORM::GetTimeMs();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::DeleteRecording(const PVR_RECORDING &recording)
{
  char            command[1200];
  string          result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 1200, "DeleteRecordedTV:%s\n", recording.strRecordingId);

  result = SendCommand(command);

  if(result.find("True") ==  string::npos)
  {
    KODI->Log(LOG_ERROR, "Deleting recording %s [failed]", recording.strRecordingId);
    return PVR_ERROR_FAILED;
  }
  KODI->Log(LOG_DEBUG, "Deleting recording %s [done]", recording.strRecordingId);

  // Although Kodi initiates the deletion of this recording, we still have to trigger Kodi to update its
  // recordings list to remove the recording at the Kodi side
  PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::RenameRecording(const PVR_RECORDING &recording)
{
  char           command[1200];
  string         result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 1200, "UpdateRecording:%s|%s\n",
    recording.strRecordingId,
    uri::encode(uri::PATH_TRAITS, recording.strTitle).c_str());

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    KODI->Log(LOG_ERROR, "RenameRecording(%s) to %s [failed]", recording.strRecordingId, recording.strTitle);
    return PVR_ERROR_FAILED;
  }
  KODI->Log(LOG_DEBUG, "RenameRecording(%s) to %s [done]", recording.strRecordingId, recording.strTitle);

  // Although Kodi initiates the rename of this recording, we still have to trigger Kodi to update its
  // recordings list to see the renamed recording at the Kodi side
  PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  if ( g_iTVServerKodiBuild < 117 )
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  char           command[512];
  string         result;

  snprintf(command, 512, "SetRecordingTimesWatched:%i|%i\n", atoi(recording.strRecordingId), count);

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    KODI->Log(LOG_ERROR, "%s: id=%s to %i [failed]", __FUNCTION__, recording.strRecordingId, count);
    return PVR_ERROR_FAILED;
  }

  KODI->Log(LOG_DEBUG, "%s: id=%s to %i [successful]", __FUNCTION__, recording.strRecordingId, count);
  PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition)
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

  snprintf(command, 512, "SetRecordingStopTime:%i|%i\n", atoi(recording.strRecordingId), lastplayedposition);

  result = SendCommand(command);

  if(result.find("True") == string::npos)
  {
    KODI->Log(LOG_ERROR, "%s: id=%s to %i [failed]", __FUNCTION__, recording.strRecordingId, lastplayedposition);
    return PVR_ERROR_FAILED;
  }

  KODI->Log(LOG_DEBUG, "%s: id=%s to %i [successful]", __FUNCTION__, recording.strRecordingId, lastplayedposition);
  PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

int cPVRClientMediaPortal::GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  if ( g_iTVServerKodiBuild < 121 )
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  char           command[512];
  string         result;
  int            lastplayedposition;

  snprintf(command, 512, "GetRecordingStopTime:%i\n", atoi(recording.strRecordingId));

  result = SendCommand(command);

  if(result.find("-1") != string::npos)
  {
    KODI->Log(LOG_ERROR, "%s: id=%s fetching stoptime [failed]", __FUNCTION__, recording.strRecordingId);
    return 0;
  }

  lastplayedposition = atoi(result.c_str());

  KODI->Log(LOG_DEBUG, "%s: id=%s stoptime=%i {s} [successful]", __FUNCTION__, recording.strRecordingId, lastplayedposition);

  return lastplayedposition;
}

/************************************************************/
/** Timer handling */

int cPVRClientMediaPortal::GetNumTimers(void)
{
  string            result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("GetScheduleCount:\n");

  return atol(result.c_str());
}

PVR_ERROR cPVRClientMediaPortal::GetTimers(ADDON_HANDLE handle)
{
  vector<string>  lines;
  string          result;
  PVR_TIMER       tag;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  result = SendCommand("ListSchedules:True\n");

  if (result.length() > 0)
  {
    Tokenize(result, lines, ",");

    memset(&tag, 0, sizeof(PVR_TIMER));

    for (vector<string>::iterator it = lines.begin(); it != lines.end(); ++it)
    {
      string& data(*it);
      uri::decode(data);

      KODI->Log(LOG_DEBUG, "SCHEDULED: %s", data.c_str() );

      cTimer timer;
      timer.SetGenreTable(m_genretable);

      if(timer.ParseLine(data.c_str()) == true)
      {
        timer.GetPVRtimerinfo(tag);
        PVR->TransferTimerEntry(handle, &tag);
      }
    }
  }

  if ( P8PLATFORM::GetTimeMs() >  m_iLastRecordingUpdate + 15000)
  {
    PVR->TriggerRecordingUpdate();
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetTimerInfo(unsigned int timernumber, PVR_TIMER &timerinfo)
{
  string         result;
  char           command[256];

  KODI->Log(LOG_DEBUG, "->GetTimerInfo(%u)", timernumber);

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  snprintf(command, 256, "GetScheduleInfo:%u\n", timernumber);

  result = SendCommand(command);

  if (result.length() == 0)
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer;
  if( timer.ParseLine(result.c_str()) == false )
  {
    KODI->Log(LOG_DEBUG, "GetTimerInfo(%i) parsing server response failed. Response: %s", timernumber, result.c_str());
    return PVR_ERROR_SERVER_ERROR;
  }

  timer.GetPVRtimerinfo(timerinfo);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
  int maxsize = *size; // the size of the types[] array when this functon is called
  int& count = *size;  // the amount of filled items in the types[] array
  count = 0;

  if (Timer::lifetimeValues == NULL)
    return PVR_ERROR_FAILED;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  //Note: schedule priority support is not implemented here
  //      The MediaPortal TV Server database has a priority field but their wiki
  //      says: "This feature is yet to be enabled".

  // One-shot epg-based (maps to MediaPortal 'Once')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::Once;
  types[count].iAttributes = MPTV_RECORD_ONCE;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30110)); /* Record once */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series weekly epg-based (maps to MediaPortal 'EveryTimeOnThisChannel')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::EveryTimeOnThisChannel;
  types[count].iAttributes = MPTV_RECORD_EVERY_TIME_ON_THIS_CHANNEL;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30115)); /* Record every time on this channel */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series weekly epg-based (maps to MediaPortal 'EveryTimeOnEveryChannel')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::EveryTimeOnEveryChannel;
  types[count].iAttributes = MPTV_RECORD_EVERY_TIME_ON_EVERY_CHANNEL;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30116)); /* Record every time on every channel */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series weekly epg-based (maps to MediaPortal 'Weekly')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::Weekly;
  types[count].iAttributes = MPTV_RECORD_WEEKLY;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30117)); /* "Record every week at this time" */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series daily epg-based (maps to MediaPortal 'Daily')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::Daily;
  types[count].iAttributes = MPTV_RECORD_DAILY;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30118)); /* Record every day at this time */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series Weekends epg-based (maps to MediaPortal 'WorkingDays')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::WorkingDays;
  types[count].iAttributes = MPTV_RECORD_WORKING_DAYS;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30114)); /* Record weekdays */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series Weekends epg-based (maps to MediaPortal 'Weekends')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::Weekends;
  types[count].iAttributes = MPTV_RECORD_WEEEKENDS;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30113)); /* Record Weekends */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
    return PVR_ERROR_NO_ERROR;

  // Series weekly epg-based (maps to MediaPortal 'WeeklyEveryTimeOnThisChannel')
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::WeeklyEveryTimeOnThisChannel;
  types[count].iAttributes = MPTV_RECORD_WEEKLY_EVERY_TIME_ON_THIS_CHANNEL;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30119)); /* Weekly on this channel */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  if (count > maxsize)
	  return PVR_ERROR_NO_ERROR;

  /* Kodi specific 'Manual' schedule type */
  memset(&types[count], 0, sizeof(types[count]));
  types[count].iId = cKodiTimerTypeOffset + TvDatabase::KodiManual;
  types[count].iAttributes = MPTV_RECORD_MANUAL;
  PVR_STRCPY(types[count].strDescription, KODI->GetLocalizedString(30122)); /* Manual */
  Timer::lifetimeValues->SetLifeTimeValues(types[count]);
  count++;

  return PVR_ERROR_NO_ERROR;
}


PVR_ERROR cPVRClientMediaPortal::AddTimer(const PVR_TIMER &timerinfo)
{
  string         result;

#ifdef _TIME32_T_DEFINED
  KODI->Log(LOG_DEBUG, "->AddTimer Channel: %i, starttime: %i endtime: %i program: %s", timerinfo.iClientChannelUid, timerinfo.startTime, timerinfo.endTime, timerinfo.strTitle);
#else
  KODI->Log(LOG_DEBUG, "->AddTimer Channel: %i, 64 bit times not yet supported!", timerinfo.iClientChannelUid);
#endif

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer(timerinfo);

  if (g_bEnableOldSeriesDlg && (timerinfo.startTime > 0) &&
      (timerinfo.iEpgUid != PVR_TIMER_NO_EPG_UID) &&
      ((timerinfo.iTimerType - cKodiTimerTypeOffset) == (unsigned int) TvDatabase::Once)
      )
  {
    /* New scheduled recording, not an instant or manual recording
     * Present a custom dialog with advanced recording settings
     */
    std::string strChannelName;
    if (timerinfo.iClientChannelUid >= 0)
    {
      strChannelName = m_channelNames[timerinfo.iClientChannelUid];
    }
    CGUIDialogRecordSettings dlgRecSettings( timerinfo, timer, strChannelName);

    int dlogResult = dlgRecSettings.DoModal();

    if (dlogResult == 0)
      return PVR_ERROR_NO_ERROR;						// user canceled timer in dialog
  }

  result = SendCommand(timer.AddScheduleCommand());

  if(result.find("True") ==  string::npos)
  {
    KODI->Log(LOG_DEBUG, "AddTimer for channel: %i [failed]", timerinfo.iClientChannelUid);
    return PVR_ERROR_FAILED;
  }
  KODI->Log(LOG_DEBUG, "AddTimer for channel: %i [done]", timerinfo.iClientChannelUid);

  // Although Kodi adds this timer, we still have to trigger Kodi to update its timer list to
  // see this new timer at the Kodi side
  PVR->TriggerTimerUpdate();
  if ( timerinfo.startTime <= 0)
  {
    // Refresh the recordings list to see the newly created recording
    usleep(100000);
    PVR->TriggerRecordingUpdate();
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::DeleteTimer(const PVR_TIMER &timer, bool UNUSED(bForceDelete))
{
  char           command[256];
  string         result;

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  // Check if this timer has a parent schedule and a program id
  // When true, it has no real schedule at the Mediaportal side.
  // The best we can do in that case is disable the timer for this program only
  if ((timer.iParentClientIndex > 0) && (timer.iEpgUid > 0))
  {
    // Don't delete this timer, but disable it only
    PVR_TIMER disableMe = timer;
    disableMe.state = PVR_TIMER_STATE_DISABLED;
    return UpdateTimer(disableMe);
  }

  cTimer mepotimer(timer);

  snprintf(command, 256, "DeleteSchedule:%i\n", mepotimer.Index());

  KODI->Log(LOG_DEBUG, "DeleteTimer: About to delete MediaPortal schedule index=%i", mepotimer.Index());
  result = SendCommand(command);

  if(result.find("True") ==  string::npos)
  {
    KODI->Log(LOG_DEBUG, "DeleteTimer %i [failed]", mepotimer.Index());
    return PVR_ERROR_FAILED;
  }
  KODI->Log(LOG_DEBUG, "DeleteTimer %i [done]", mepotimer.Index());

  // Although Kodi deletes this timer, we still have to trigger Kodi to update its timer list to
  // remove the timer from the Kodi list
  PVR->TriggerTimerUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::UpdateTimer(const PVR_TIMER &timerinfo)
{
  string         result;

#ifdef _TIME32_T_DEFINED
  KODI->Log(LOG_DEBUG, "->UpdateTimer Index: %i Channel: %i, starttime: %i endtime: %i program: %s", timerinfo.iClientIndex, timerinfo.iClientChannelUid, timerinfo.startTime, timerinfo.endTime, timerinfo.strTitle);
#else
  KODI->Log(LOG_DEBUG, "->UpdateTimer Channel: %i, 64 bit times not yet supported!", timerinfo.iClientChannelUid);
#endif

  if (!IsUp())
    return PVR_ERROR_SERVER_ERROR;

  cTimer timer(timerinfo);

  result = SendCommand(timer.UpdateScheduleCommand());
  if(result.find("True") ==  string::npos)
  {
    KODI->Log(LOG_DEBUG, "UpdateTimer for channel: %i [failed]", timerinfo.iClientChannelUid);
    return PVR_ERROR_FAILED;
  }
  KODI->Log(LOG_DEBUG, "UpdateTimer for channel: %i [done]", timerinfo.iClientChannelUid);

  // Although Kodi changes this timer, we still have to trigger Kodi to update its timer list to
  // see the timer changes at the Kodi side
  PVR->TriggerTimerUpdate();

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
bool cPVRClientMediaPortal::OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
  string result;
  char   command[256] = "";
  const char* sResolveRTSPHostname = booltostring(g_bResolveRTSPHostname);
  vector<string> timeshiftfields;

  KODI->Log(LOG_NOTICE, "Open Live stream for channel uid=%i", channelinfo.iUniqueId);
  if (!IsUp())
  {
    m_iCurrentChannel = -1;
    m_bTimeShiftStarted = false;
    m_bSkipCloseLiveStream = false; //initialization
    m_signalStateCounter = 0;
    KODI->Log(LOG_ERROR, "Open Live stream failed. No connection to backend.");
    return false;
  }

  if (((int)channelinfo.iUniqueId) == m_iCurrentChannel)
  {
    KODI->Log(LOG_NOTICE, "New channel uid equal to the already streaming channel. Skipping re-tune.");
    return true;
  }

  m_iCurrentChannel = -1; // make sure that it is not a valid channel nr in case it will fail lateron
  m_signalStateCounter = 0;
  m_bTimeShiftStarted = false;
  m_bSkipCloseLiveStream = false; //initialization

  // Start the timeshift
  // Use the optimized TimeshiftChannel call (don't stop a running timeshift)
  snprintf(command, 256, "TimeshiftChannel:%i|%s|False\n", channelinfo.iUniqueId, sResolveRTSPHostname);
  result = SendCommand(command);

  if (result.find("ERROR") != std::string::npos || result.length() == 0)
  {
    KODI->Log(LOG_ERROR, "Could not start the timeshift for channel uid=%i. Reason: %s", channelinfo.iUniqueId, result.c_str());
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

        int tvresult = atoi(timeshiftfields[1].c_str());
        // Display one of the localized error messages 30060-30075
        KODI->QueueNotification(QUEUE_ERROR, KODI->GetLocalizedString(30059 + tvresult));
      }
      else
      {
         KODI->QueueNotification(QUEUE_ERROR, result.c_str());
      }
    }
    else
    {
      if (result.find("[ERROR]: TVServer answer: ") != std::string::npos)
      {
        //Skip first part: "[ERROR]: TVServer answer: "
        KODI->QueueNotification(QUEUE_ERROR, "TVServer: %s", result.substr(26).c_str());
      }
      else
      {
        //Skip first part: "[ERROR]: "
        KODI->QueueNotification(QUEUE_ERROR, result.substr(7).c_str());
      }
    }
    m_iCurrentChannel = -1;
    if (m_tsreader != nullptr)
    {
      SAFE_DELETE(m_tsreader);
    }
    return false;
  }
  else
  {
    Tokenize(result, timeshiftfields, "|");

    if(timeshiftfields.size()<4)
    {
      KODI->Log(LOG_ERROR, "OpenLiveStream: Field count mismatch (<4). Data: %s\n", result.c_str());
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
    if (g_eStreamingMethod == TSReader)
    {
      KODI->Log(LOG_NOTICE, "Channel timeshift buffer: %s", timeshiftfields[2].c_str());
      if (channelinfo.bIsRadio)
      {
        usleep(100000); // 100 ms sleep to allow the buffer to fill
      }
    }
    else
    {
      KODI->Log(LOG_NOTICE, "Channel stream URL: %s", m_PlaybackURL.c_str());
    }

    if (g_iSleepOnRTSPurl > 0)
    {
      KODI->Log(LOG_NOTICE, "Sleeping %i ms before opening stream: %s", g_iSleepOnRTSPurl, timeshiftfields[0].c_str());
      usleep(g_iSleepOnRTSPurl * 1000);
    }

    // Check the returned stream URL. When the URL is an rtsp stream, we need
    // to close it again after watching to stop the timeshift.
    // A radio web stream (added to the TV Server) will return the web stream
    // URL without starting a timeshift.
    if(timeshiftfields[0].compare(0,4, "rtsp") == 0)
    {
      m_bTimeShiftStarted = true;
    }

    if (g_eStreamingMethod == TSReader)
    {
      if (m_tsreader != NULL)
      {
        bool bReturn = false;

        // Continue with the existing TsReader.
        KODI->Log(LOG_NOTICE, "Re-using existing TsReader...");
        //if(g_bDirectTSFileRead)
        if(g_bUseRTSP == false)
        {
          m_tsreader->SetCardId(atoi(timeshiftfields[3].c_str()));

          if ((g_iTVServerKodiBuild >=110) && (timeshiftfields.size()>=6))
            bReturn = m_tsreader->OnZap(timeshiftfields[2].c_str(), atoll(timeshiftfields[4].c_str()), atol(timeshiftfields[5].c_str()));
          else
            bReturn = m_tsreader->OnZap(timeshiftfields[2].c_str(), -1, -1);
        }
        else
        {
          // RTSP url
          KODI->Log(LOG_NOTICE, "Skipping OnZap for TSReader RTSP");
          bReturn = true; //Fast forward seek (OnZap) does not work for RTSP
        }

        if (bReturn)
        {
          m_iCurrentChannel = (int) channelinfo.iUniqueId;
          m_iCurrentCard = atoi(timeshiftfields[3].c_str());
          m_bCurrentChannelIsRadio = channelinfo.bIsRadio;
        }
        else
        {
          KODI->Log(LOG_ERROR, "Re-using the existing TsReader failed.");
          CloseLiveStream();
        }

        return bReturn;
      }
      else
      {
        KODI->Log(LOG_NOTICE, "Creating a new TsReader...");
        m_tsreader = new CTsReader();
      }

      if (!g_bUseRTSP)
      {
        // Reading directly from the Timeshift buffer
        m_tsreader->SetCardSettings(&m_cCards);
        m_tsreader->SetCardId(atoi(timeshiftfields[3].c_str()));

        //if (g_szTimeshiftDir.length() > 0)
        //  m_tsreader->SetDirectory(g_szTimeshiftDir);

        if ( m_tsreader->Open(timeshiftfields[2].c_str()) != S_OK )
        {
          KODI->Log(LOG_ERROR, "Cannot open timeshift buffer %s", timeshiftfields[2].c_str());
          CloseLiveStream();
          return false;
        }
      }
      else
      {
        // use the RTSP url and live555
        if ( m_tsreader->Open(timeshiftfields[0].c_str()) != S_OK)
        {
          KODI->Log(LOG_ERROR, "Cannot open channel url %s", timeshiftfields[0].c_str());
          CloseLiveStream();
          return false;
        }
        usleep(400000);
      }
    }

    // at this point everything is ready for playback
    m_iCurrentChannel = (int) channelinfo.iUniqueId;
    m_iCurrentCard = atoi(timeshiftfields[3].c_str());
    m_bCurrentChannelIsRadio = channelinfo.bIsRadio;
  }
  KODI->Log(LOG_NOTICE, "OpenLiveStream: success for channel id %i (%s) on card %i", m_iCurrentChannel, channelinfo.strChannelName, m_iCurrentCard);

  return true;
}

int cPVRClientMediaPortal::ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  size_t read_wanted = iBufferSize;
  size_t read_done   = 0;
  static int read_timeouts  = 0;
  unsigned char* bufptr = pBuffer;

  //KODI->Log(LOG_DEBUG, "->ReadLiveStream(buf_size=%i)", buf_size);
  if (g_eStreamingMethod != TSReader)
  {
    KODI->Log(LOG_ERROR, "ReadLiveStream: this function should not be called in FFMPEG/RTSP mode. Use 'Reset the PVR database' to re-read the channel list");
    return 0;
  }

  if (!m_tsreader)
  {
    KODI->Log(LOG_ERROR, "ReadLiveStream: failed. No open TSReader");
    return -1;
  }

  while (read_done < static_cast<size_t>(iBufferSize))
  {
    read_wanted = iBufferSize - read_done;

    if (m_tsreader->Read(bufptr, read_wanted, &read_wanted) > 0)
    {
      usleep(20000);
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
          KODI->Log(LOG_NOTICE, "Kodi requested %u bytes, but the TSReader got only %lu bytes in 2 seconds", iBufferSize, read_done);
        }
        read_timeouts = 0;

        //TODO
        //if read_done == 0 then check if the backend is still timeshifting,
        //or retrieve the reason why the timeshifting was stopped/failed...

        return static_cast<int>(read_done);
      }
      bufptr += read_wanted;
      read_timeouts++;
      usleep(10000);
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
    if (g_eStreamingMethod == TSReader && m_tsreader)
    {
      m_tsreader->Close();
      SAFE_DELETE(m_tsreader);
    }
    result = SendCommand("StopTimeshift:\n");
    KODI->Log(LOG_NOTICE, "CloseLiveStream: %s", result.c_str());
    m_bTimeShiftStarted = false;
    m_iCurrentChannel = -1;
    m_iCurrentCard = -1;
    m_PlaybackURL.clear();

    m_signalStateCounter = 0;
  }
}

long long cPVRClientMediaPortal::SeekLiveStream(long long iPosition, int iWhence)
{
  if (g_eStreamingMethod == ffmpeg || !m_tsreader)
  {
    KODI->Log(LOG_ERROR, "SeekLiveStream: is not supported in FFMPEG/RTSP mode.");
    return -1;
  }

  if (iPosition == 0 && iWhence == SEEK_CUR)
  {
    return m_tsreader->GetFilePointer();
  }
  return m_tsreader->SetFilePointer(iPosition, iWhence);
}

long long cPVRClientMediaPortal::LengthLiveStream(void)
{
  if (g_eStreamingMethod == ffmpeg || !m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

bool cPVRClientMediaPortal::IsRealTimeStream(void)
{
  return m_bTimeShiftStarted;
}

PVR_ERROR cPVRClientMediaPortal::SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
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

  signalStatus.iSignal = m_iSignal;
  signalStatus.iSNR = m_iSNR;
  signalStatus.iBER = m_signalStateCounter;
  PVR_STRCPY(signalStatus.strAdapterStatus, "timeshifting"); // hardcoded for now...


  if (m_iCurrentCard >= 0)
  {
    // Try to determine the name of the tv/radio card from the local card cache
    Card currentCard;
    if (m_cCards.GetCard(m_iCurrentCard, currentCard) == true)
    {
      PVR_STRCPY(signalStatus.strAdapterName, currentCard.Name.c_str());
      return PVR_ERROR_NO_ERROR;
    }
  }

  PVR_STRCLR(signalStatus.strAdapterName);

  return PVR_ERROR_NO_ERROR;
}


/************************************************************/
/** Record stream handling */
// MediaPortal recordings are also rtsp streams. Main difference here with
// respect to the live tv streams is that the URLs for the recordings
// can be requested on beforehand (done in the TVServerKodi plugin).

bool cPVRClientMediaPortal::OpenRecordedStream(const PVR_RECORDING &recording)
{
  KODI->Log(LOG_NOTICE, "OpenRecordedStream (id=%s, RTSP=%d)", recording.strRecordingId, (g_bUseRTSP ? "true" : "false"));

  m_bTimeShiftStarted = false;

  if (!IsUp())
    return false;

  if (g_eStreamingMethod == ffmpeg)
  {
    KODI->Log(LOG_ERROR, "Addon is in 'ffmpeg' mode. Kodi should play the RTSP url directly. Please reset your Kodi PVR database!");
    return false;
  }

  std::string recfile = "";

#if 0
  // TVServerKodi v1.1.0.90 or higher
  string         result;
  char           command[256];

  //if(g_bUseRecordingsDir)
  if(!g_bUseRTSP)
    snprintf(command, 256, "GetRecordingInfo:%s|False\n", recording.strRecordingId);
  else
    snprintf(command, 256, "GetRecordingInfo:%s|True\n", recording.strRecordingId);
  result = SendCommand(command);

  if (result.empty())
  {
    KODI->Log(LOG_ERROR, "Backend command '%s' returned a zero-length answer.", command);
    return false;
  }

  cRecording myrecording;
  if (!myrecording.ParseLine(result))
  {
    KODI->Log(LOG_ERROR, "Parsing result from '%s' command failed. Result='%s'.", command, result.c_str());
    return false;
  }

  KODI->Log(LOG_NOTICE, "RECORDING: %s", result.c_str() );
#endif
  cRecording* myrecording = GetRecordingInfo(recording);

  if (!myrecording)
  {
    return false;
  }

  if (!g_bUseRTSP)
  {
    recfile  = myrecording->FilePath();
    if (recfile.empty())
    {
      KODI->Log(LOG_ERROR, "Backend returned an empty recording filename for recording id %s.", recording.strRecordingId);
      recfile = myrecording->Stream();
      if (!recfile.empty())
      {
        KODI->Log(LOG_NOTICE, "Trying to use the recording RTSP stream URL name instead.");
      }
    }
  }
  else
  {
    recfile = myrecording->Stream();
    if (recfile.empty())
    {
      KODI->Log(LOG_ERROR, "Backend returned an empty RTSP stream URL for recording id %s.", recording.strRecordingId);
      recfile = myrecording->FilePath();
      if (!recfile.empty())
      {
        KODI->Log(LOG_NOTICE, "Trying to use the filename instead.");
      }
    }
  }

  if (recfile.empty())
  {
    KODI->Log(LOG_ERROR, "Recording playback not possible. Backend returned an empty filename and no RTSP stream URL for recording id %s", recording.strRecordingId);
    KODI->QueueNotification(QUEUE_ERROR, KODI->GetLocalizedString(30052));
    // Tell Kodi to re-read the list with recordings to remove deleted/non-existing recordings as a result of backend auto-deletion.
    PVR->TriggerRecordingUpdate();
    return false;
  }

  // We have a recording file name or RTSP url, time to open it...
  m_tsreader = new CTsReader();
  m_tsreader->SetCardSettings(&m_cCards);
  if ( m_tsreader->Open(recfile.c_str()) != S_OK )
    return false;

  return true;
}

void cPVRClientMediaPortal::CloseRecordedStream(void)
{
  if (!IsUp() || g_eStreamingMethod == ffmpeg)
     return;

  if (m_tsreader)
  {
    KODI->Log(LOG_NOTICE, "CloseRecordedStream: Stop TSReader...");
    m_tsreader->Close();
    SAFE_DELETE(m_tsreader);
  }
  else
  {
    KODI->Log(LOG_DEBUG, "CloseRecordedStream: Nothing to do.");
  }
}

int cPVRClientMediaPortal::ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  size_t read_wanted = static_cast<size_t>(iBufferSize);
  size_t read_done   = 0;
  unsigned char* bufptr = pBuffer;

  if (g_eStreamingMethod == ffmpeg)
    return -1;

  while (read_done < static_cast<size_t>(iBufferSize))
  {
    read_wanted = iBufferSize - read_done;
    if (!m_tsreader)
      return -1;

    if (m_tsreader->Read(bufptr, read_wanted, &read_wanted) > 0)
    {
      usleep(20000);
      return static_cast<int>(read_wanted);
    }
    read_done += read_wanted;

    if ( read_done < static_cast<size_t>(iBufferSize) )
    {
      bufptr += read_wanted;
      usleep(20000);
    }
  }

  return static_cast<int>(read_done);
}

long long cPVRClientMediaPortal::SeekRecordedStream(long long iPosition, int iWhence)
{
  if (g_eStreamingMethod == ffmpeg || !m_tsreader)
  {
    return -1;
  }
#ifdef _DEBUG
  KODI->Log(LOG_DEBUG, "SeekRec: Current pos %lli", m_tsreader->GetFilePointer());
#endif
  KODI->Log(LOG_DEBUG,"SeekRec: iWhence %i pos %lli", iWhence, iPosition);

  return m_tsreader->SetFilePointer(iPosition, iWhence);
}

long long  cPVRClientMediaPortal::LengthRecordedStream(void)
{
  if (g_eStreamingMethod == ffmpeg || !m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

PVR_ERROR cPVRClientMediaPortal::GetRecordingStreamProperties(const PVR_RECORDING* recording,
                                                              PVR_NAMED_VALUE* properties,
                                                              unsigned int* iPropertiesCount)
{
  // GetRecordingStreamProperties is called before OpenRecordedStream
  // If we return a stream URL here, Kodi will use its internal player to open the stream and bypass the PVR addon
  *iPropertiesCount = 0;

  cRecording* myrecording = GetRecordingInfo(*recording);

  if (!myrecording)
    return PVR_ERROR_NO_ERROR;

  AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_MIMETYPE, "video/mp2t");

  if (g_eStreamingMethod == ffmpeg)
  {
    AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, myrecording->Stream());
  }
  else if (!g_bUseRTSP)
  {
    if (myrecording->IsRecording() == false)
    {
#ifdef TARGET_WINDOWS_DESKTOP
      if (OS::CFile::Exists(myrecording->FilePath()))
      {
        std::string recordingUri(ToKodiPath(myrecording->FilePath()));

        // Direct file playback by Kodi (without involving the addon)
        AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, recordingUri.c_str());
      }
#endif
    }
    else
    {
      // Indicate that this is a real-time stream
      AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cPVRClientMediaPortal::GetChannelStreamProperties(const PVR_CHANNEL* channel,
                                                            PVR_NAMED_VALUE* properties,
                                                            unsigned int* iPropertiesCount)
{
  *iPropertiesCount = 0;
  if (g_eStreamingMethod == ffmpeg)
  {
    // GetChannelStreamProperties is called before OpenLiveStream by Kodi, so we should already open the stream here...
    // The actual call to OpenLiveStream will return immediately since we've already tuned the correct channel here.
    if (m_bTimeShiftStarted == true)
    {
      //CloseLiveStream();
    }
    if (OpenLiveStream(*channel) == true)
    {
      if (!m_PlaybackURL.empty())
      {
        KODI->Log(LOG_DEBUG, "GetChannelStreamProperties (ffmpeg) for uid=%i is '%s'", channel->iUniqueId,
                  m_PlaybackURL.c_str());
        AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, m_PlaybackURL);
        AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");
        AddStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_MIMETYPE, "video/mp2t");
        return PVR_ERROR_NO_ERROR;
      }
    }
  }
  else if (g_eStreamingMethod == TSReader)
  {
    if ((m_bTimeShiftStarted == true) && (g_bFastChannelSwitch = true))
    {
      // This ignores the next CloseLiveStream call from Kodi to speedup channel switching
      m_bSkipCloseLiveStream = true;
    }
  }
  else
  {
    KODI->Log(LOG_ERROR, "GetChannelStreamProperties for uid=%i returned no URL",
              channel->iUniqueId);
  }

  *iPropertiesCount = 0;

  return PVR_ERROR_NO_ERROR;
}

void cPVRClientMediaPortal::PauseStream(bool UNUSED(bPaused))
{
  if (m_tsreader)
    m_tsreader->Pause();
}

bool cPVRClientMediaPortal::CanPauseAndSeek()
{
  if (m_tsreader)
    return true;
  else
    return false;
}

void cPVRClientMediaPortal::LoadGenreTable()
{
  // Read the genre string to type/subtype translation file:
  if(g_bReadGenre)
  {
    string sGenreFile = g_szUserPath + PATH_SEPARATOR_CHAR + "resources" + PATH_SEPARATOR_CHAR + "genre_translation.xml";

    if (!KODI->FileExists(sGenreFile.c_str(), false))
    {
      sGenreFile = g_szUserPath + PATH_SEPARATOR_CHAR + "genre_translation.xml";
      if (!KODI->FileExists(sGenreFile.c_str(), false))
      {
        sGenreFile = g_szClientPath + PATH_SEPARATOR_CHAR + "resources" + PATH_SEPARATOR_CHAR + "genre_translation.xml";
      }
    }

    m_genretable = new CGenreTable(sGenreFile);
  }
}

void cPVRClientMediaPortal::LoadCardSettings()
{
  KODI->Log(LOG_DEBUG, "Loading card settings");

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
    KODI->Log(LOG_DEBUG, "Connection state changed to '%s'",
      GetConnectionStateString(newState));
    m_state = newState;

    /* Notify connection state change (callback!) */
    PVR->ConnectionStateChange(GetConnectionString(), m_state, NULL);
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

cRecording* cPVRClientMediaPortal::GetRecordingInfo(const PVR_RECORDING & recording)
{
  // Is this the same recording as the previous one?
  if (m_lastSelectedRecording)
  {
    int recId = atoi(recording.strRecordingId);
    if (m_lastSelectedRecording->Index() == recId)
    {
      return m_lastSelectedRecording;
    }
    SAFE_DELETE(m_lastSelectedRecording);
  }

  if (!IsUp())
    return nullptr;

  string result;
  string command;

  command = StringUtils::Format("GetRecordingInfo:%s|%s|False|%s\n", 
    recording.strRecordingId, 
    ((g_bUseRTSP || g_eStreamingMethod == ffmpeg) ? "True" : "False"),
    g_bResolveRTSPHostname ? "True" : "False"
  );
  result = SendCommand(command);

  if (result.empty())
  {
    KODI->Log(LOG_ERROR, "Backend command '%s' returned a zero-length answer.", command.c_str());
    return nullptr;
  }

  m_lastSelectedRecording = new cRecording();
  if (!m_lastSelectedRecording->ParseLine(result))
  {
    KODI->Log(LOG_ERROR, "Parsing result from '%s' command failed. Result='%s'.", command.c_str(), result.c_str());
    return nullptr;
  }
  KODI->Log(LOG_NOTICE, "RECORDING: %s", result.c_str());
  return m_lastSelectedRecording;
}

PVR_ERROR cPVRClientMediaPortal::GetStreamTimes(PVR_STREAM_TIMES* stream_times)
{
  if (!m_bTimeShiftStarted && m_lastSelectedRecording)
  {
    // Recording playback
    // Warning: documentation in xbmc_pvr_types.h is wrong. pts values are not in seconds.
    stream_times->startTime = 0; // seconds
    stream_times->ptsStart = 0;  // Unit must match Kodi's internal m_clock.GetClock() which is in useconds
    if (m_lastSelectedRecording->IsRecording())
    {
      stream_times->ptsBegin = m_tsreader->GetPtsBegin();  // useconds
      stream_times->ptsEnd = m_tsreader->GetPtsEnd();
    }
    else
    {
       stream_times->ptsBegin = 0;  // useconds
       stream_times->ptsEnd = ((int64_t)m_lastSelectedRecording->Duration()) * DVD_TIME_BASE; //useconds
    }
    return PVR_ERROR_NO_ERROR;
  }
  else if (m_bTimeShiftStarted)
  {
    stream_times->startTime = m_tsreader->GetStartTime();
    stream_times->ptsStart = 0;  // Unit must match Kodi's internal m_clock.GetClock() which is in useconds
    stream_times->ptsBegin = m_tsreader->GetPtsBegin();  // useconds
    stream_times->ptsEnd = m_tsreader->GetPtsEnd();
    return PVR_ERROR_NO_ERROR;
  }
  *stream_times = { 0 };

  return PVR_ERROR_NOT_IMPLEMENTED;
}

void cPVRClientMediaPortal::AddStreamProperty(PVR_NAMED_VALUE* properties,
                       unsigned int* propertiesCount,
                       std::string name,
                       std::string value)
{
  PVR_STRCPY(properties[*propertiesCount].strName, name.c_str());
  PVR_STRCPY(properties[*propertiesCount].strValue, value.c_str());
  *propertiesCount = (*propertiesCount) + 1;
}

