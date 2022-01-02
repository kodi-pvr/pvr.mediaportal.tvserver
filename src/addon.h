/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include "kodi/AddonBase.h"

class ATTR_DLL_LOCAL CPVRMediaPortalAddon : public kodi::addon::CAddonBase
{
public:
  CPVRMediaPortalAddon() = default;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;
  ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                              KODI_ADDON_INSTANCE_HDL& hdl) override;
};

#endif /* CLIENT_H */
