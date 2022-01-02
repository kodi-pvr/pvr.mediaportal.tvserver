/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "addon.h"
#include "pvrclient-mediaportal.h"
#include "settings.h"

ADDON_STATUS CPVRMediaPortalAddon::SetSetting(const std::string& settingName,
                                              const kodi::addon::CSettingValue& settingValue)
{
  return CSettings::Get().SetSetting(settingName, settingValue);
}

ADDON_STATUS CPVRMediaPortalAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                                  KODI_ADDON_INSTANCE_HDL& hdl)
{
  if (instance.IsType(ADDON_INSTANCE_PVR))
  {
    kodi::Log(ADDON_LOG_INFO, "Creating MediaPortal PVR-Client");

    CSettings::Get().Load();

    /* Connect to ARGUS TV */
    cPVRClientMediaPortal* client = new cPVRClientMediaPortal(instance);
    hdl = client;

    ADDON_STATUS curStatus = client->TryConnect();
    if (curStatus == ADDON_STATUS_PERMANENT_FAILURE)
    {
      return ADDON_STATUS_UNKNOWN;
    }
    else if (curStatus == ADDON_STATUS_LOST_CONNECTION)
    {
      // The addon will try to reconnect, so don't show the permanent failure.
      return ADDON_STATUS_OK;
    }

    return curStatus;
  }

  return ADDON_STATUS_UNKNOWN;
}

ADDONCREATOR(CPVRMediaPortalAddon)
