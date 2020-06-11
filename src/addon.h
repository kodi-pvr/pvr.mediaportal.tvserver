/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include "kodi/AddonBase.h"

class ATTRIBUTE_HIDDEN CPVRMediaPortalAddon : public kodi::addon::CAddonBase
{
public:
  CPVRMediaPortalAddon() = default;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::CSettingValue& settingValue) override;
  ADDON_STATUS CreateInstance(int instanceType,
                              const std::string& instanceID,
                              KODI_HANDLE instance,
                              const std::string& version,
                              KODI_HANDLE& addonInstance) override;
};

#endif /* CLIENT_H */
