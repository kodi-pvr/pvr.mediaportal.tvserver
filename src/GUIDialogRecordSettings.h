/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2013 Marcel Groothuis
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "timers.h"

#include <kodi/addon-instance/pvr/Timers.h>
#include <kodi/gui/Window.h>
#include <kodi/gui/controls/Spin.h>

class CGUIDialogRecordSettings : public kodi::gui::CWindow
{
public:
  CGUIDialogRecordSettings(const kodi::addon::PVRTimer& timerinfo, cTimer& timer, const std::string& channelName);
  virtual ~CGUIDialogRecordSettings() = default;

  bool Show();
  void Close();
  int DoModal();  // returns -1 => load failed, 0 => cancel, 1 => ok

private:
  int m_retVal;  // -1 => load failed, 0 => cancel, 1 => ok

  bool OnClick(int controlId) override;
  bool OnFocus(int controlId) override;
  bool OnInit() override;
  bool OnAction(int actionId, uint32_t buttoncode, wchar_t unicode) override;

  // Specific for this dialog:
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinFrequency;
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinAirtime;
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinChannels;
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinKeep;
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinPreRecord;
  std::shared_ptr<kodi::gui::controls::CSpin> m_spinPostRecord;

  void UpdateTimerSettings(void);

  /* Enumerated types corresponding with the spincontrol values */
  enum RecordingFrequency
  {
    Once = 0,
    Daily = 1,
    Weekly = 2,
    Weekends = 3,
    WeekDays = 4
  };

  enum RecordingAirtime
  {
    ThisTime = 0,
    AnyTime = 1
  };

  enum RecordingChannels
  {
    ThisChannel = 0,
    AnyChannel = 1
  };

  /* Private members */
  std::string m_channel;
  std::string m_startTime;
  std::string m_startDate;
  std::string m_endTime;
  std::string m_title;

  RecordingFrequency m_frequency;
  RecordingAirtime   m_airtime;
  RecordingChannels  m_channels;

  const kodi::addon::PVRTimer& m_timerinfo;
  cTimer& m_timer;
};

