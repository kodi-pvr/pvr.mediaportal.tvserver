/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <p8-platform/util/StringUtils.h>
#include "client.h"
#include "kodi/xbmc_pvr_dll.h"
#include "pvrclient-mediaportal.h"
#include "utils.h"
#include "timers.h"

using namespace std;
using namespace ADDON;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string      g_szHostname           = DEFAULT_HOST;                  ///< The Host name or IP of the MediaPortal TV Server
int              g_iPort                = DEFAULT_PORT;                  ///< The TVServerKodi listening port (default: 9596)
int              g_iConnectTimeout      = DEFAULT_TIMEOUT;               ///< The Socket connection timeout
int              g_iSleepOnRTSPurl      = DEFAULT_SLEEP_RTSP_URL;        ///< An optional delay between tuning a channel and opening the corresponding RTSP stream in Kodi (default: 0)
bool             g_bOnlyFTA             = DEFAULT_FTA_ONLY;              ///< Send only Free-To-Air Channels inside Channel list to Kodi
bool             g_bRadioEnabled        = DEFAULT_RADIO;                 ///< Send also Radio channels list to Kodi
bool             g_bHandleMessages      = DEFAULT_HANDLE_MSG;            ///< Send VDR's OSD status messages to Kodi OSD
bool             g_bResolveRTSPHostname = DEFAULT_RESOLVE_RTSP_HOSTNAME; ///< Resolve the server hostname in the rtsp URLs to an IP at the TV Server side (default: false)
bool             g_bReadGenre           = DEFAULT_READ_GENRE;            ///< Read the genre strings from MediaPortal and translate them into Kodi DVB genre id's (only English)
bool             g_bEnableOldSeriesDlg  = false;                         ///< Show the old pre-Jarvis series recording dialog
std::string      g_szTVGroup            = DEFAULT_TVGROUP;               ///< Import only TV channels from this TV Server TV group
std::string      g_szRadioGroup         = DEFAULT_RADIOGROUP;            ///< Import only radio channels from this TV Server radio group
std::string      g_szSMBusername        = DEFAULT_SMBUSERNAME;           ///< Windows user account used to access share
std::string      g_szSMBpassword        = DEFAULT_SMBPASSWORD;           ///< Windows user password used to access share
                                                                         ///< Leave empty to use current user when running on Windows
eStreamingMethod            g_eStreamingMethod          = TSReader;
TvDatabase::KeepMethodType  g_KeepMethodType            = TvDatabase::Always;
int                         g_DefaultRecordingLifeTime  = 100;           ///< The default days which are configured in kodi
bool                        g_bFastChannelSwitch        = true;          ///< Don't stop an existing timeshift on a channel switch
bool                        g_bUseRTSP                  = false;         ///< Use RTSP streaming when using the tsreader

/* Client member variables */
ADDON_STATUS            m_curStatus    = ADDON_STATUS_UNKNOWN;
cPVRClientMediaPortal  *g_client       = NULL;
std::string             g_szUserPath   = "";
std::string             g_szClientPath = "";
CHelper_libXBMC_addon  *KODI           = NULL;
CHelper_libXBMC_pvr    *PVR            = NULL;
CHelper_libKODI_guilib *GUI = NULL;

extern "C" {

void ADDON_ReadSettings(void);

/***********************************************************
 * Standard AddOn related public library functions
 ***********************************************************/

//-- Create -------------------------------------------------------------------
// Called after loading of the dll, all steps to become Client functional
// must be performed here.
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
  {
    m_curStatus = ADDON_STATUS_UNKNOWN;
    return m_curStatus;
  }

  AddonProperties_PVR* pvrprops = (AddonProperties_PVR*)props;

  KODI = new CHelper_libXBMC_addon;
  if (!KODI->RegisterMe(hdl))
  {
    SAFE_DELETE(KODI);
    m_curStatus = ADDON_STATUS_PERMANENT_FAILURE;
    return m_curStatus;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(KODI);
    m_curStatus = ADDON_STATUS_PERMANENT_FAILURE;
    return m_curStatus;
  }

  GUI = new CHelper_libKODI_guilib;
  if (!GUI->RegisterMe(hdl))
  {
    SAFE_DELETE(GUI);
    SAFE_DELETE(PVR);
    SAFE_DELETE(KODI);
    m_curStatus = ADDON_STATUS_PERMANENT_FAILURE;
    return m_curStatus;
  }

  KODI->Log(LOG_INFO, "Creating MediaPortal PVR-Client");

  m_curStatus    = ADDON_STATUS_UNKNOWN;
  g_szUserPath   = pvrprops->strUserPath;
  g_szClientPath = pvrprops->strClientPath;

  ADDON_ReadSettings();

  /* Create connection to MediaPortal Kodi TV client */
  g_client       = new cPVRClientMediaPortal();

  m_curStatus = g_client->TryConnect();
  if (m_curStatus == ADDON_STATUS_PERMANENT_FAILURE)
  {
    SAFE_DELETE(g_client);
    SAFE_DELETE(GUI);
    SAFE_DELETE(PVR);
    SAFE_DELETE(KODI);
  }
  else if (m_curStatus == ADDON_STATUS_LOST_CONNECTION)
  {
    // The addon will try to reconnect, so don't show the permanent failure.
    return ADDON_STATUS_OK;
  }

  return m_curStatus;
}

//-- Destroy ------------------------------------------------------------------
// Used during destruction of the client, all steps to do clean and safe Create
// again must be done.
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  SAFE_DELETE(g_client);
  SAFE_DELETE(GUI);
  SAFE_DELETE(PVR);
  SAFE_DELETE(KODI);

  m_curStatus = ADDON_STATUS_UNKNOWN;
}

//-- GetStatus ----------------------------------------------------------------
// Report the current Add-On Status to Kodi
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  /* check whether we're still connected */
  if (m_curStatus == ADDON_STATUS_OK && g_client && !g_client->IsUp())
    m_curStatus = ADDON_STATUS_LOST_CONNECTION;

  return m_curStatus;
}

void ADDON_ReadSettings(void)
{
  /* Read setting "host" from settings.xml */
  char buffer[1024];

  if (!KODI)
    return;

  /* Connection settings */
  /***********************/
  if (KODI->GetSetting("host", &buffer))
  {
    g_szHostname = buffer;
    uri::decode(g_szHostname);
  }
  else
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'host' setting, falling back to '127.0.0.1' as default");
    g_szHostname = DEFAULT_HOST;
  }

  /* Read setting "port" from settings.xml */
  if (!KODI->GetSetting("port", &g_iPort))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'port' setting, falling back to '9596' as default");
    g_iPort = DEFAULT_PORT;
  }

  /* Read setting "timeout" from settings.xml */
  if (!KODI->GetSetting("timeout", &g_iConnectTimeout))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'timeout' setting, falling back to %i seconds as default", DEFAULT_TIMEOUT);
    g_iConnectTimeout = DEFAULT_TIMEOUT;
  }

  /* MediaPortal settings */
  /***********************/

  /* Read setting "ftaonly" from settings.xml */
  if (!KODI->GetSetting("ftaonly", &g_bOnlyFTA))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'ftaonly' setting, falling back to 'false' as default");
    g_bOnlyFTA = DEFAULT_FTA_ONLY;
  }

  /* Read setting "useradio" from settings.xml */
  if (!KODI->GetSetting("useradio", &g_bRadioEnabled))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'useradio' setting, falling back to 'true' as default");
    g_bRadioEnabled = DEFAULT_RADIO;
  }

  if (!KODI->GetSetting("tvgroup", &buffer))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'tvgroup' setting, falling back to '' as default");
  } else {
    g_szTVGroup = buffer;
    StringUtils::Replace(g_szTVGroup,";","|");
  }

  if (!KODI->GetSetting("radiogroup", &buffer))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'radiogroup' setting, falling back to '' as default");
  } else {
    g_szRadioGroup = buffer;
    StringUtils::Replace(g_szRadioGroup,";","|");
  }

  if (!KODI->GetSetting("streamingmethod", &g_eStreamingMethod))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'streamingmethod' setting, falling back to 'tsreader' as default");
    g_eStreamingMethod = TSReader;
  }

  /* Read setting "resolvertsphostname" from settings.xml */
  if (!KODI->GetSetting("resolvertsphostname", &g_bResolveRTSPHostname))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'resolvertsphostname' setting, falling back to 'true' as default");
    g_bResolveRTSPHostname = DEFAULT_RESOLVE_RTSP_HOSTNAME;
  }

  /* Read setting "readgenre" from settings.xml */
  if (!KODI->GetSetting("readgenre", &g_bReadGenre))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'readgenre' setting, falling back to 'true' as default");
    g_bReadGenre = DEFAULT_READ_GENRE;
  }

  /* Read setting "readgenre" from settings.xml */
  if (!KODI->GetSetting("enableoldseriesdlg", &g_bEnableOldSeriesDlg))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'enableoldseriesdlg' setting, falling back to 'false' as default");
    g_bEnableOldSeriesDlg = false;
  }

  /* Read setting "keepmethodtype" from settings.xml */
  if (!KODI->GetSetting("keepmethodtype", &g_KeepMethodType))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'keepmethodtype' setting, falling back to 'Always' as default");
    g_KeepMethodType = TvDatabase::Always;
  }

  /* Read setting "defaultrecordinglifetime" from settings.xml */
  if (!KODI->GetSetting("defaultrecordinglifetime", &g_DefaultRecordingLifeTime))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'defaultrecordinglifetime' setting, falling back to '100' as default");
    g_DefaultRecordingLifeTime = 100;
  }

  /* Read setting "sleeponrtspurl" from settings.xml */
  if (!KODI->GetSetting("sleeponrtspurl", &g_iSleepOnRTSPurl))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'sleeponrtspurl' setting, falling back to %i seconds as default", DEFAULT_SLEEP_RTSP_URL);
    g_iSleepOnRTSPurl = DEFAULT_SLEEP_RTSP_URL;
  }


  /* TSReader settings */
  /*********************/
  /* Read setting "fastchannelswitch" from settings.xml */
  if (!KODI->GetSetting("fastchannelswitch", &g_bFastChannelSwitch))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'fastchannelswitch' setting, falling back to 'false' as default");
    g_bFastChannelSwitch = false;
  }

  /* read setting "user" from settings.xml */
  if (!KODI->GetSetting("smbusername", &buffer))
  {
    KODI->Log(LOG_ERROR, "Couldn't get 'smbusername' setting, falling back to '%s' as default", DEFAULT_SMBUSERNAME);
    g_szSMBusername = DEFAULT_SMBUSERNAME;
  }
  else
    g_szSMBusername = buffer;

  /* read setting "pass" from settings.xml */
  if (!KODI->GetSetting("smbpassword", &buffer))
  {
    KODI->Log(LOG_ERROR, "Couldn't get 'smbpassword' setting, falling back to '%s' as default", DEFAULT_SMBPASSWORD);
    g_szSMBpassword = DEFAULT_SMBPASSWORD;
  }
  else
    g_szSMBpassword = buffer;

  /* Read setting "usertsp" from settings.xml */
  if (!KODI->GetSetting("usertsp", &g_bUseRTSP))
  {
    /* If setting is unknown fallback to defaults */
    KODI->Log(LOG_ERROR, "Couldn't get 'usertsp' setting, falling back to 'false' as default");
    g_bUseRTSP = false;
  }

  /* Log the current settings for debugging purposes */
  KODI->Log(LOG_DEBUG, "settings: streamingmethod: %s, usertsp=%i", (( g_eStreamingMethod == TSReader) ? "TSReader" : "ffmpeg"), (int) g_bUseRTSP);
  KODI->Log(LOG_DEBUG, "settings: host='%s', port=%i, timeout=%i", g_szHostname.c_str(), g_iPort, g_iConnectTimeout);
  KODI->Log(LOG_DEBUG, "settings: ftaonly=%i, useradio=%i, tvgroup='%s', radiogroup='%s'", (int) g_bOnlyFTA, (int) g_bRadioEnabled, g_szTVGroup.c_str(), g_szRadioGroup.c_str());
  KODI->Log(LOG_DEBUG, "settings: readgenre=%i, enableoldseriesdlg=%i, sleeponrtspurl=%i", (int)g_bReadGenre, (int)g_bEnableOldSeriesDlg, g_iSleepOnRTSPurl);
  KODI->Log(LOG_DEBUG, "settings: resolvertsphostname=%i", (int) g_bResolveRTSPHostname);
  KODI->Log(LOG_DEBUG, "settings: fastchannelswitch=%i", (int) g_bFastChannelSwitch);
  KODI->Log(LOG_DEBUG, "settings: smb user='%s', pass=%s", g_szSMBusername.c_str(), (g_szSMBpassword.length() > 0 ? "<set>" : "<empty>"));
  KODI->Log(LOG_DEBUG, "settings: keepmethodtype=%i, defaultrecordinglifetime=%i", (int)g_KeepMethodType, (int)g_DefaultRecordingLifeTime);
}

//-- SetSetting ---------------------------------------------------------------
// Called everytime a setting is changed by the user and to inform AddOn about
// new setting and to do required stuff to apply it.
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  string str = settingName;

  // SetSetting can occur when the addon is enabled, but TV support still
  // disabled. In that case the addon is not loaded, so we should not try
  // to change its settings.
  if (!KODI)
    return ADDON_STATUS_OK;

  if (str == "host")
  {
    string tmp_sHostname;
    KODI->Log(LOG_INFO, "Changed Setting 'host' from %s to %s", g_szHostname.c_str(), (const char*) settingValue);
    tmp_sHostname = g_szHostname;
    g_szHostname = (const char*) settingValue;
    if (tmp_sHostname != g_szHostname)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (str == "port")
  {
    KODI->Log(LOG_INFO, "Changed Setting 'port' from %u to %u", g_iPort, *(int*) settingValue);
    if (g_iPort != *(int*) settingValue)
    {
      g_iPort = *(int*) settingValue;
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "ftaonly")
  {
    KODI->Log(LOG_INFO, "Changed setting 'ftaonly' from %u to %u", g_bOnlyFTA, *(bool*) settingValue);
    g_bOnlyFTA = *(bool*) settingValue;
  }
  else if (str == "useradio")
  {
    KODI->Log(LOG_INFO, "Changed setting 'useradio' from %u to %u", g_bRadioEnabled, *(bool*) settingValue);
    g_bRadioEnabled = *(bool*) settingValue;
  }
  else if (str == "timeout")
  {
    KODI->Log(LOG_INFO, "Changed setting 'timeout' from %u to %u", g_iConnectTimeout, *(int*) settingValue);
    g_iConnectTimeout = *(int*) settingValue;
  }
  else if (str == "tvgroup")
  {
    KODI->Log(LOG_INFO, "Changed setting 'tvgroup' from '%s' to '%s'", g_szTVGroup.c_str(), (const char*) settingValue);
    g_szTVGroup = (const char*) settingValue;
  }
  else if (str == "radiogroup")
  {
    KODI->Log(LOG_INFO, "Changed setting 'radiogroup' from '%s' to '%s'", g_szRadioGroup.c_str(), (const char*) settingValue);
    g_szRadioGroup = (const char*) settingValue;
  }
  else if (str == "resolvertsphostname")
  {
    KODI->Log(LOG_INFO, "Changed setting 'resolvertsphostname' from %u to %u", g_bResolveRTSPHostname, *(bool*) settingValue);
    g_bResolveRTSPHostname = *(bool*) settingValue;
  }
  else if (str == "readgenre")
  {
    KODI->Log(LOG_INFO, "Changed setting 'readgenre' from %u to %u", g_bReadGenre, *(bool*) settingValue);
    g_bReadGenre = *(bool*) settingValue;
  }
  else if (str == "enableoldseriesdlg")
  {
    KODI->Log(LOG_INFO, "Changed setting 'enableoldseriesdlg' from %u to %u", g_bEnableOldSeriesDlg, *(bool*)settingValue);
    g_bEnableOldSeriesDlg = *(bool*)settingValue;
  }
  else if (str == "keepmethodtype")
  {
    if (g_KeepMethodType != *(TvDatabase::KeepMethodType*)settingValue)
    {
      KODI->Log(LOG_INFO, "Changed setting 'keepmethodtype' from %u to %u", g_KeepMethodType, *(int*)settingValue);
      g_KeepMethodType = *(TvDatabase::KeepMethodType*)settingValue;
    }
  }
  else if (str == "defaultrecordinglifetime")
  {
    if (g_DefaultRecordingLifeTime != *(int*)settingValue)
    {
      KODI->Log(LOG_INFO, "Changed setting 'defaultrecordinglifetime' from %u to %u", g_DefaultRecordingLifeTime, *(int*)settingValue);
      g_DefaultRecordingLifeTime = *(int*)settingValue;
    }
  }
  else if (str == "sleeponrtspurl")
  {
    KODI->Log(LOG_INFO, "Changed setting 'sleeponrtspurl' from %u to %u", g_iSleepOnRTSPurl, *(int*) settingValue);
    g_iSleepOnRTSPurl = *(int*) settingValue;
  }
  else if (str == "smbusername")
  {
    KODI->Log(LOG_INFO, "Changed setting 'smbusername' from '%s' to '%s'", g_szSMBusername.c_str(), (const char*) settingValue);
    g_szSMBusername = (const char*) settingValue;
  }
  else if (str == "smbpassword")
  {
    KODI->Log(LOG_INFO, "Changed setting 'smbpassword' from '%s' to '%s'", g_szSMBpassword.c_str(), (const char*) settingValue);
   g_szSMBpassword = (const char*) settingValue;
  }
  else if (str == "fastchannelswitch")
  {
    KODI->Log(LOG_INFO, "Changed setting 'fastchannelswitch' from %u to %u", g_bFastChannelSwitch, *(bool*) settingValue);
    g_bFastChannelSwitch = *(bool*) settingValue;
  }
  else if (str == "streamingmethod")
  {
    if (g_eStreamingMethod != *(eStreamingMethod*) settingValue)
    {
      KODI->Log(LOG_INFO, "Changed setting 'streamingmethod' from %u to %u", g_eStreamingMethod, *(int*) settingValue);
      g_eStreamingMethod = *(eStreamingMethod*) settingValue;
      /* Switching between ffmpeg and tsreader mode requires a restart due to different channel streams */
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "usertsp")
  {
    KODI->Log(LOG_INFO, "Changed setting 'usertsp' from %u to %u", g_bUseRTSP, *(bool*) settingValue);
    g_bUseRTSP = *(bool*) settingValue;
  }

  return ADDON_STATUS_OK;
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

//-- GetCapabilities -----------------------------------------------------
// Tell Kodi our requirements
//-----------------------------------------------------------------------------
PVR_ERROR GetCapabilities(PVR_ADDON_CAPABILITIES *pCapabilities)
{
  KODI->Log(LOG_DEBUG, "->GetCapabilities()");

  memset(pCapabilities, 0, sizeof(PVR_ADDON_CAPABILITIES));

  pCapabilities->bSupportsEPG = true;
  pCapabilities->bSupportsEPGEdl = false;
  pCapabilities->bSupportsTV = true;
  pCapabilities->bSupportsRadio = g_bRadioEnabled;
  pCapabilities->bSupportsRecordings = true;
  pCapabilities->bSupportsRecordingsUndelete = false;
  pCapabilities->bSupportsTimers = true;
  pCapabilities->bSupportsChannelGroups = true;
  pCapabilities->bSupportsChannelScan = false;
  pCapabilities->bSupportsChannelSettings = false;
  pCapabilities->bHandlesInputStream = true;
  pCapabilities->bHandlesDemuxing = false;
  pCapabilities->bSupportsRecordingPlayCount = (g_iTVServerKodiBuild < 117) ? false : true;
  pCapabilities->bSupportsLastPlayedPosition = (g_iTVServerKodiBuild < 121) ? false : true;
  pCapabilities->bSupportsRecordingEdl = false;
  pCapabilities->bSupportsRecordingsRename = true;
  pCapabilities->bSupportsRecordingsLifetimeChange = false;
  pCapabilities->bSupportsDescrambleInfo = false;
  pCapabilities->bSupportsAsyncEPGTransfer = false;
  pCapabilities->iRecordingsLifetimesSize = 0;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* UNUSED(pProperties))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

//-- GetBackendName -----------------------------------------------------------
// Return the Name of the Backend
//-----------------------------------------------------------------------------
const char * GetBackendName(void)
{
  if (g_client)
    return g_client->GetBackendName();
  else
    return "";
}

//-- GetBackendVersion --------------------------------------------------------
// Return the Version of the Backend as String
//-----------------------------------------------------------------------------
const char * GetBackendVersion(void)
{
  if (g_client)
    return g_client->GetBackendVersion();
  else
    return "";
}

//-- GetConnectionString ------------------------------------------------------
// Return a String with connection info, if available
//-----------------------------------------------------------------------------
const char * GetConnectionString(void)
{
  if (g_client)
    return g_client->GetConnectionString();
  else
    return "addon error!";
}

//-- GetBackendHostname -------------------------------------------------------
// Return a String with the backend host name
//-----------------------------------------------------------------------------
const char * GetBackendHostname(void)
{
  return g_szHostname.c_str();
}

//-- GetDriveSpace ------------------------------------------------------------
// Return the Total and Free Drive space on the PVR Backend
//-----------------------------------------------------------------------------
PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetDriveSpace(iTotal, iUsed);
}

PVR_ERROR GetBackendTime(time_t *localTime, int *gmtOffset)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetBackendTime(localTime, gmtOffset);
}

PVR_ERROR OpenDialogChannelScan()
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR CallMenuHook(const PVR_MENUHOOK& UNUSED(menuhook), const PVR_MENUHOOK_DATA& UNUSED(item))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}


/*******************************************/
/** PVR EPG Functions                     **/

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, int iChannelUid, time_t iStart, time_t iEnd)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetEpg(handle, iChannelUid, iStart, iEnd);
}


/*******************************************/
/** PVR Channel Functions                 **/

int GetChannelsAmount()
{
  if (!g_client)
    return 0;
  else
    return g_client->GetNumChannels();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetChannels(handle, bRadio);
}

PVR_ERROR DeleteChannel(const PVR_CHANNEL& UNUSED(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR RenameChannel(const PVR_CHANNEL& UNUSED(channel))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL& UNUSED(channelinfo))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL& UNUSED(channelinfo))
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}


/*******************************************/
/** PVR Channel group Functions           **/

int GetChannelGroupsAmount(void)
{
  if (!g_client)
    return 0;
  else
    return g_client->GetChannelGroupsAmount();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetChannelGroups(handle, bRadio);
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetChannelGroupMembers(handle, group);
}


/*******************************************/
/** PVR Recording Functions               **/

int GetRecordingsAmount(bool deleted)
{
  if (!g_client)
    return 0;
  else
    return g_client->GetNumRecordings();
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetRecordings(handle);
}

PVR_ERROR DeleteRecording(const PVR_RECORDING &recording)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->DeleteRecording(recording);
}

PVR_ERROR RenameRecording(const PVR_RECORDING &recording)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->RenameRecording(recording);
}

PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->SetRecordingPlayCount(recording, count);
}

PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->SetRecordingLastPlayedPosition(recording, lastplayedposition);
}

int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetRecordingLastPlayedPosition(recording);
}

/*******************************************/
/** PVR Timer Functions                   **/

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetTimerTypes(types, size);
}

int GetTimersAmount(void)
{
  if (!g_client)
    return 0;
  else
    return g_client->GetNumTimers();
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->GetTimers(handle);
}

PVR_ERROR AddTimer(const PVR_TIMER &timer)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->AddTimer(timer);
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->DeleteTimer(timer, bForceDelete);
}

PVR_ERROR UpdateTimer(const PVR_TIMER &timer)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->UpdateTimer(timer);
}


/*******************************************/
/** PVR Live Stream Functions             **/

bool OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
  if (!g_client)
    return false;
  else
    return g_client->OpenLiveStream(channelinfo);
}

void CloseLiveStream()
{
  if (g_client)
    g_client->CloseLiveStream();
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (!g_client)
    return 0;
  else
    return g_client->ReadLiveStream(pBuffer, iBufferSize);
}

long long SeekLiveStream(long long iPosition, int iWhence)
{
  if (!g_client)
    return -1;
  else
    return g_client->SeekLiveStream(iPosition, iWhence);
}

long long LengthLiveStream(void)
{
  if (!g_client)
    return -1;
  else
    return g_client->LengthLiveStream();
}

bool IsRealTimeStream(void)
{
  if (g_client == NULL)
    return false;

  return g_client->IsRealTimeStream();
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  if (!g_client)
    return PVR_ERROR_SERVER_ERROR;
  else
    return g_client->SignalStatus(signalStatus);
}

/*******************************************/
/** PVR Recording Stream Functions        **/

bool OpenRecordedStream(const PVR_RECORDING &recording)
{
  if (!g_client)
    return false;
  else
    return g_client->OpenRecordedStream(recording);
}

void CloseRecordedStream(void)
{
  if (g_client)
    g_client->CloseRecordedStream();
}

int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (!g_client)
    return 0;
  else
    return g_client->ReadRecordedStream(pBuffer, iBufferSize);
}

long long SeekRecordedStream(long long iPosition, int iWhence)
{
  if (!g_client)
    return -1;
  else
    return g_client->SeekRecordedStream(iPosition, iWhence);
}

long long LengthRecordedStream(void)
{
  if (!g_client)
    return -1;
  else
    return g_client->LengthRecordedStream();
}

bool CanPauseStream(void)
{
  if (g_client)
    return g_client->CanPauseAndSeek();

  return false;
}

void PauseStream(bool bPaused)
{
  if (g_client)
    g_client->PauseStream(bPaused);
}

bool CanSeekStream(void)
{
  if (g_client)
    return g_client->CanPauseAndSeek();

  return false;
}

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  if ((!channel) || (!properties) || (!iPropertiesCount) || (!g_client))
  {
    return PVR_ERROR_FAILED;
  }

  return g_client->GetChannelStreamProperties(channel, properties, iPropertiesCount);
}

PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING* recording, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  if ((!recording) || (!properties) || (!iPropertiesCount) || (!g_client))
  {
    return PVR_ERROR_FAILED;
  }

  return g_client->GetRecordingStreamProperties(recording, properties, iPropertiesCount);
}

PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES* stream_times)
{
  if ((!stream_times) || (!g_client))
  {
    return PVR_ERROR_INVALID_PARAMETERS;
  }

  return g_client->GetStreamTimes(stream_times);
}

PVR_ERROR GetStreamReadChunkSize(int* chunksize)
{
  if ((!chunksize) || (!g_client))
  {
    return PVR_ERROR_INVALID_PARAMETERS;
  }

  return g_client->GetStreamReadChunkSize(chunksize);
}


/** UNUSED API FUNCTIONS */
DemuxPacket* DemuxRead(void) { return NULL; }
void DemuxAbort(void) {}
void DemuxReset(void) {}
void DemuxFlush(void) {}
void FillBuffer(bool mode) {}

PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
bool SeekTime(double,bool,double*) { return false; }
void SetSpeed(int) {};
PVR_ERROR UndeleteRecording(const PVR_RECORDING& UNUSED(recording)) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingSize(const PVR_RECORDING* recording, int64_t* sizeInBytes) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetDescrambleInfo(PVR_DESCRAMBLE_INFO*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }

} //end extern "C"
