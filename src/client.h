/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "kodi/libKODI_guilib.h"
#include "timers.h"

enum eStreamingMethod
{
  TSReader = 0,
  ffmpeg = 1
};

#define DEFAULT_HOST                  "127.0.0.1"
#define DEFAULT_PORT                  9596
#define DEFAULT_FTA_ONLY              false
#define DEFAULT_RADIO                 true
#define DEFAULT_TIMEOUT               10
#define DEFAULT_HANDLE_MSG            false
#define DEFAULT_RESOLVE_RTSP_HOSTNAME false
#define DEFAULT_READ_GENRE            false
#define DEFAULT_SLEEP_RTSP_URL        0
#define DEFAULT_USE_REC_DIR           false
#define DEFAULT_TVGROUP               ""
#define DEFAULT_RADIOGROUP            ""
#define DEFAULT_DIRECT_TS_FR          false
#define DEFAULT_SMBUSERNAME           "Guest"
#define DEFAULT_SMBPASSWORD           ""

extern std::string      g_szUserPath;         ///< The Path to the user directory inside user profile
extern std::string      g_szClientPath;       ///< The Path where this driver is located

/* Client Settings */
extern std::string      g_szHostname;
extern int              g_iPort;
extern int              g_iConnectTimeout;
extern int              g_iSleepOnRTSPurl;
extern bool             g_bOnlyFTA;
extern bool             g_bRadioEnabled;
extern bool             g_bHandleMessages;
extern bool             g_bResolveRTSPHostname;
extern bool             g_bReadGenre;
extern bool             g_bEnableOldSeriesDlg;
extern bool             g_bFastChannelSwitch;
extern bool             g_bUseRTSP;           ///< Use RTSP streaming when using the tsreader
extern std::string      g_szTVGroup;
extern std::string      g_szRadioGroup;
extern std::string      g_szSMBusername;
extern std::string      g_szSMBpassword;
extern eStreamingMethod g_eStreamingMethod;
extern TvDatabase::KeepMethodType  g_KeepMethodType;
extern int                         g_DefaultRecordingLifeTime;

extern ADDON::CHelper_libXBMC_addon *KODI;
extern CHelper_libXBMC_pvr          *PVR;
extern CHelper_libKODI_guilib       *GUI;

extern int              g_iTVServerKodiBuild;

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

#endif /* CLIENT_H */
