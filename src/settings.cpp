/*
 *  Copyright (C) 2020-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "settings.h"

#include "uri.h"

#include <algorithm>

CSettings& CSettings::Get()
{
  static CSettings settings;
  return settings;
}

bool CSettings::Load()
{
  /* Connection settings */
  /***********************/
  if (kodi::addon::CheckSettingString("host", m_szHostname))
  {
    uri::decode(m_szHostname);
  }
  else
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'host' setting, falling back to '127.0.0.1' as default");
    m_szHostname = DEFAULT_HOST;
  }

  /* Read setting "port" from settings.xml */
  if (!kodi::addon::CheckSettingInt("port", m_iPort))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'port' setting, falling back to '9596' as default");
    m_iPort = DEFAULT_PORT;
  }

  /* Read setting "timeout" from settings.xml */
  if (!kodi::addon::CheckSettingInt("timeout", m_iConnectTimeout))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'timeout' setting, falling back to %i seconds as default",
              DEFAULT_TIMEOUT);
    m_iConnectTimeout = DEFAULT_TIMEOUT;
  }

  /* MediaPortal settings */
  /***********************/

  /* Read setting "ftaonly" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("ftaonly", m_bOnlyFTA))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'ftaonly' setting, falling back to 'false' as default");
    m_bOnlyFTA = DEFAULT_FTA_ONLY;
  }

  /* Read setting "useradio" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("useradio", m_bRadioEnabled))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'useradio' setting, falling back to 'true' as default");
    m_bRadioEnabled = DEFAULT_RADIO;
  }

  if (!kodi::addon::CheckSettingString("tvgroup", m_szTVGroup))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'tvgroup' setting, falling back to '' as default");
  }
  else
  {
    std::replace(m_szTVGroup.begin(), m_szTVGroup.end(), ';', '|');
  }

  if (!kodi::addon::CheckSettingString("radiogroup", m_szRadioGroup))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR, "Couldn't get 'radiogroup' setting, falling back to '' as default");
  }
  else
  {
    std::replace(m_szRadioGroup.begin(), m_szRadioGroup.end(), ';', '|');
  }

  if (!kodi::addon::CheckSettingEnum<eStreamingMethod>("streamingmethod", m_eStreamingMethod))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'streamingmethod' setting, falling back to 'tsreader' as default");
    m_eStreamingMethod = TSReader;
  }

  /* Read setting "resolvertsphostname" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("resolvertsphostname", m_bResolveRTSPHostname))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'resolvertsphostname' setting, falling back to 'true' as default");
    m_bResolveRTSPHostname = DEFAULT_RESOLVE_RTSP_HOSTNAME;
  }

  /* Read setting "readgenre" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("readgenre", m_bReadGenre))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'readgenre' setting, falling back to 'true' as default");
    m_bReadGenre = DEFAULT_READ_GENRE;
  }

  /* Read setting "readgenre" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("enableoldseriesdlg", m_bEnableOldSeriesDlg))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'enableoldseriesdlg' setting, falling back to 'false' as default");
    m_bEnableOldSeriesDlg = false;
  }

  /* Read setting "keepmethodtype" from settings.xml */
  if (!kodi::addon::CheckSettingEnum<TvDatabase::KeepMethodType>("keepmethodtype", m_KeepMethodType))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'keepmethodtype' setting, falling back to 'Always' as default");
    m_KeepMethodType = TvDatabase::Always;
  }

  /* Read setting "defaultrecordinglifetime" from settings.xml */
  if (!kodi::addon::CheckSettingInt("defaultrecordinglifetime", m_DefaultRecordingLifeTime))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'defaultrecordinglifetime' setting, falling back to '100' as default");
    m_DefaultRecordingLifeTime = 100;
  }

  /* Read setting "sleeponrtspurl" from settings.xml */
  if (!kodi::addon::CheckSettingInt("sleeponrtspurl", m_iSleepOnRTSPurl))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'sleeponrtspurl' setting, falling back to %i seconds as default",
              DEFAULT_SLEEP_RTSP_URL);
    m_iSleepOnRTSPurl = DEFAULT_SLEEP_RTSP_URL;
  }


  /* TSReader settings */
  /*********************/
  /* Read setting "fastchannelswitch" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("fastchannelswitch", m_bFastChannelSwitch))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'fastchannelswitch' setting, falling back to 'false' as default");
    m_bFastChannelSwitch = false;
  }

  /* read setting "user" from settings.xml */
  if (!kodi::addon::CheckSettingString("smbusername", m_szSMBusername))
  {
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'smbusername' setting, falling back to '%s' as default",
              DEFAULT_SMBUSERNAME);
    m_szSMBusername = DEFAULT_SMBUSERNAME;
  }

  /* read setting "pass" from settings.xml */
  if (!kodi::addon::CheckSettingString("smbpassword", m_szSMBpassword))
  {
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'smbpassword' setting, falling back to '%s' as default",
              DEFAULT_SMBPASSWORD);
    m_szSMBpassword = DEFAULT_SMBPASSWORD;
  }

  /* Read setting "usertsp" from settings.xml */
  if (!kodi::addon::CheckSettingBoolean("usertsp", m_bUseRTSP))
  {
    /* If setting is unknown fallback to defaults */
    kodi::Log(ADDON_LOG_ERROR,
              "Couldn't get 'usertsp' setting, falling back to 'false' as default");
    m_bUseRTSP = false;
  }

  /* Log the current settings for debugging purposes */
  kodi::Log(ADDON_LOG_DEBUG, "settings: streamingmethod: %s, usertsp=%i",
            ((m_eStreamingMethod == TSReader) ? "TSReader" : "ffmpeg"), (int)m_bUseRTSP);
  kodi::Log(ADDON_LOG_DEBUG, "settings: host='%s', port=%i, timeout=%i", m_szHostname.c_str(),
            m_iPort, m_iConnectTimeout);
  kodi::Log(ADDON_LOG_DEBUG, "settings: ftaonly=%i, useradio=%i, tvgroup='%s', radiogroup='%s'",
            (int)m_bOnlyFTA, (int)m_bRadioEnabled, m_szTVGroup.c_str(), m_szRadioGroup.c_str());
  kodi::Log(ADDON_LOG_DEBUG, "settings: readgenre=%i, enableoldseriesdlg=%i, sleeponrtspurl=%i",
            (int)m_bReadGenre, (int)m_bEnableOldSeriesDlg, m_iSleepOnRTSPurl);
  kodi::Log(ADDON_LOG_DEBUG, "settings: resolvertsphostname=%i", (int)m_bResolveRTSPHostname);
  kodi::Log(ADDON_LOG_DEBUG, "settings: fastchannelswitch=%i", (int)m_bFastChannelSwitch);
  kodi::Log(ADDON_LOG_DEBUG, "settings: smb user='%s', pass=%s", m_szSMBusername.c_str(),
            (m_szSMBpassword.length() > 0 ? "<set>" : "<empty>"));
  kodi::Log(ADDON_LOG_DEBUG, "settings: keepmethodtype=%i, defaultrecordinglifetime=%i",
            (int)m_KeepMethodType, (int)m_DefaultRecordingLifeTime);

  return true;
}

ADDON_STATUS CSettings::SetSetting(const std::string& settingName,
                                   const kodi::addon::CSettingValue& settingValue)
{
  if (settingName == "host")
  {
    std::string tmp_sHostname;
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'host' from %s to %s", m_szHostname.c_str(),
              settingValue.GetString().c_str());
    tmp_sHostname = m_szHostname;
    m_szHostname = settingValue.GetString();
    if (tmp_sHostname != m_szHostname)
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (settingName == "port")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed Setting 'port' from %u to %u", m_iPort,
              settingValue.GetInt());
    if (m_iPort != settingValue.GetInt())
    {
      m_iPort = settingValue.GetInt();
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "ftaonly")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'ftaonly' from %u to %u", m_bOnlyFTA,
              settingValue.GetBoolean());
    m_bOnlyFTA = settingValue.GetBoolean();
  }
  else if (settingName == "useradio")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'useradio' from %u to %u", m_bRadioEnabled,
              settingValue.GetBoolean());
    m_bRadioEnabled = settingValue.GetBoolean();
  }
  else if (settingName == "timeout")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'timeout' from %u to %u", m_iConnectTimeout,
              settingValue.GetInt());
    m_iConnectTimeout = settingValue.GetInt();
  }
  else if (settingName == "tvgroup")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'tvgroup' from '%s' to '%s'", m_szTVGroup.c_str(),
              settingValue.GetString().c_str());
    m_szTVGroup = settingValue.GetString();
  }
  else if (settingName == "radiogroup")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'radiogroup' from '%s' to '%s'",
              m_szRadioGroup.c_str(), settingValue.GetString().c_str());
    m_szRadioGroup = settingValue.GetString();
  }
  else if (settingName == "resolvertsphostname")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'resolvertsphostname' from %u to %u",
              m_bResolveRTSPHostname, settingValue.GetBoolean());
    m_bResolveRTSPHostname = settingValue.GetBoolean();
  }
  else if (settingName == "readgenre")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'readgenre' from %u to %u", m_bReadGenre,
              settingValue.GetBoolean());
    m_bReadGenre = settingValue.GetBoolean();
  }
  else if (settingName == "enableoldseriesdlg")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'enableoldseriesdlg' from %u to %u",
              m_bEnableOldSeriesDlg, settingValue.GetBoolean());
    m_bEnableOldSeriesDlg = settingValue.GetBoolean();
  }
  else if (settingName == "keepmethodtype")
  {
    if (m_KeepMethodType != settingValue.GetEnum<TvDatabase::KeepMethodType>())
    {
      kodi::Log(ADDON_LOG_INFO, "Changed setting 'keepmethodtype' from %u to %u", m_KeepMethodType,
                settingValue.GetEnum<TvDatabase::KeepMethodType>());
      m_KeepMethodType = settingValue.GetEnum<TvDatabase::KeepMethodType>();
    }
  }
  else if (settingName == "defaultrecordinglifetime")
  {
    if (m_DefaultRecordingLifeTime != settingValue.GetInt())
    {
      kodi::Log(ADDON_LOG_INFO, "Changed setting 'defaultrecordinglifetime' from %u to %u",
                m_DefaultRecordingLifeTime, settingValue.GetInt());
      m_DefaultRecordingLifeTime = settingValue.GetInt();
    }
  }
  else if (settingName == "sleeponrtspurl")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'sleeponrtspurl' from %u to %u", m_iSleepOnRTSPurl,
              settingValue.GetInt());
    m_iSleepOnRTSPurl = settingValue.GetInt();
  }
  else if (settingName == "smbusername")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'smbusername' from '%s' to '%s'",
              m_szSMBusername.c_str(), settingValue.GetString().c_str());
    m_szSMBusername = settingValue.GetString();
  }
  else if (settingName == "smbpassword")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'smbpassword' from '%s' to '%s'",
              m_szSMBpassword.c_str(), settingValue.GetString().c_str());
    m_szSMBpassword = settingValue.GetString();
  }
  else if (settingName == "fastchannelswitch")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'fastchannelswitch' from %u to %u",
              m_bFastChannelSwitch, settingValue.GetBoolean());
    m_bFastChannelSwitch = settingValue.GetBoolean();
  }
  else if (settingName == "streamingmethod")
  {
    if (m_eStreamingMethod != settingValue.GetEnum<eStreamingMethod>())
    {
      kodi::Log(ADDON_LOG_INFO, "Changed setting 'streamingmethod' from %u to %u",
                m_eStreamingMethod, settingValue.GetEnum<eStreamingMethod>());
      m_eStreamingMethod = settingValue.GetEnum<eStreamingMethod>();
      /* Switching between ffmpeg and tsreader mode requires a restart due to different channel streams */
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (settingName == "usertsp")
  {
    kodi::Log(ADDON_LOG_INFO, "Changed setting 'usertsp' from %u to %u", m_bUseRTSP,
              settingValue.GetBoolean());
    m_bUseRTSP = settingValue.GetBoolean();
  }

  return ADDON_STATUS_OK;
}
