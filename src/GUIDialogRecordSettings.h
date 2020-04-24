/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2013 Marcel Groothuis
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "client.h"
#include "timers.h"

class CGUIDialogRecordSettings
{
public:
  CGUIDialogRecordSettings(const PVR_TIMER &timerinfo, cTimer& timer, const std::string& channelName);
  virtual ~CGUIDialogRecordSettings();

  bool Show();
  void Close();
  int DoModal();  // returns -1 => load failed, 0 => cancel, 1 => ok

private:
  // Following is needed for every dialog:
  CAddonGUIWindow* m_window;
  int m_retVal;  // -1 => load failed, 0 => cancel, 1 => ok

  bool OnClick(int controlId);
  bool OnFocus(int controlId);
  bool OnInit();
  bool OnAction(int actionId);

  static bool OnClickCB(GUIHANDLE cbhdl, int controlId);
  static bool OnFocusCB(GUIHANDLE cbhdl, int controlId);
  static bool OnInitCB(GUIHANDLE cbhdl);
  static bool OnActionCB(GUIHANDLE cbhdl, int actionId);

  // Specific for this dialog:
  CAddonGUISpinControl* m_spinFrequency;
  CAddonGUISpinControl* m_spinAirtime;
  CAddonGUISpinControl* m_spinChannels;
  CAddonGUISpinControl* m_spinKeep;
  CAddonGUISpinControl* m_spinPreRecord;
  CAddonGUISpinControl* m_spinPostRecord;

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

  const PVR_TIMER &m_timerinfo;
  cTimer& m_timer;
};

