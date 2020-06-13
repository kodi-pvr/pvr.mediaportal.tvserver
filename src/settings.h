/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "timers.h"

#include <kodi/AddonBase.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9596
#define DEFAULT_FTA_ONLY false
#define DEFAULT_RADIO true
#define DEFAULT_TIMEOUT 10
#define DEFAULT_HANDLE_MSG false
#define DEFAULT_RESOLVE_RTSP_HOSTNAME false
#define DEFAULT_READ_GENRE false
#define DEFAULT_SLEEP_RTSP_URL 0
#define DEFAULT_USE_REC_DIR false
#define DEFAULT_TVGROUP ""
#define DEFAULT_RADIOGROUP ""
#define DEFAULT_DIRECT_TS_FR false
#define DEFAULT_SMBUSERNAME "Guest"
#define DEFAULT_SMBPASSWORD ""

enum eStreamingMethod
{
  TSReader = 0,
  ffmpeg = 1
};

class ATTRIBUTE_HIDDEN CSettings
{
public:
  static CSettings& Get();

  bool Load();
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue);

  const std::string& GetHostname() const { return m_szHostname; }
  int GetPort() const { return m_iPort; }
  int GetConnectTimeout() const { return m_iConnectTimeout; }
  int GetSleepOnRTSPurl() const { return m_iSleepOnRTSPurl; }
  bool GetOnlyFTA() const { return m_bOnlyFTA; }
  bool GetRadioEnabled() const { return m_bRadioEnabled; }
  bool GetHandleMessages() const { return m_bHandleMessages; }
  bool GetResolveRTSPHostname() const { return m_bResolveRTSPHostname; }
  bool GetReadGenre() const { return m_bReadGenre; }
  bool GetEnableOldSeriesDlg() const { return m_bEnableOldSeriesDlg; }
  const std::string& GetTVGroup() const { return m_szTVGroup; }
  const std::string& GetRadioGroup() const { return m_szRadioGroup; }
  const std::string& GetSMBusername() const { return m_szSMBusername; }
  const std::string& GetSMBpassword() const { return m_szSMBpassword; }
  eStreamingMethod GetStreamingMethod() const { return m_eStreamingMethod; }
  TvDatabase::KeepMethodType GetKeepMethodType() const { return m_KeepMethodType; }
  int GetDefaultRecordingLifeTime() const { return m_DefaultRecordingLifeTime; }
  bool GetFastChannelSwitch() const { return m_bFastChannelSwitch; }
  bool GetUseRTSP() const { return m_bUseRTSP; }

private:
  CSettings() = default;

  /// The Host name or IP of the MediaPortal TV Server
  std::string m_szHostname = DEFAULT_HOST;
  /// The TVServerKodi listening port (default: 9596)
  int m_iPort = DEFAULT_PORT;
  /// The Socket connection timeout
  int m_iConnectTimeout = DEFAULT_TIMEOUT;
  /// An optional delay between tuning a channel and opening the corresponding
  /// RTSP stream in Kodi (default: 0)
  int m_iSleepOnRTSPurl = DEFAULT_SLEEP_RTSP_URL;
  /// Send only Free-To-Air Channels inside Channel list to Kodi
  bool m_bOnlyFTA = DEFAULT_FTA_ONLY;
  /// Send also Radio channels list to Kodi
  bool m_bRadioEnabled = DEFAULT_RADIO;
  /// Send VDR's OSD status messages to Kodi OSD
  bool m_bHandleMessages = DEFAULT_HANDLE_MSG;
  /// Resolve the server hostname in the rtsp URLs to an IP at the TV Server
  /// side (default: false)
  bool m_bResolveRTSPHostname = DEFAULT_RESOLVE_RTSP_HOSTNAME;
  /// Read the genre strings from MediaPortal and translate them into Kodi DVB
  /// genre id's (only English)
  bool m_bReadGenre = DEFAULT_READ_GENRE;
  /// Show the old pre-Jarvis series recording dialog
  bool m_bEnableOldSeriesDlg = false;
  /// Import only TV channels from this TV Server TV group
  std::string m_szTVGroup = DEFAULT_TVGROUP;
  /// Import only radio channels from this TV Server radio group
  std::string m_szRadioGroup = DEFAULT_RADIOGROUP;
  /// Windows user account used to access share
  std::string m_szSMBusername = DEFAULT_SMBUSERNAME;
  /// Windows user password used to access share Leave empty to use current user
  /// when running on Windows
  std::string m_szSMBpassword = DEFAULT_SMBPASSWORD;
  ///
  eStreamingMethod m_eStreamingMethod = TSReader;
  ///
  TvDatabase::KeepMethodType m_KeepMethodType = TvDatabase::Always;
  /// The default days which are configured in kodi
  int m_DefaultRecordingLifeTime = 100;
  /// Don't stop an existing timeshift on a channel switch
  bool m_bFastChannelSwitch = true;
  /// Use RTSP streaming when using the tsreader
  bool m_bUseRTSP = false;
};
