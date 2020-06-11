/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "GUIDialogRecordSettings.h"
#include "timers.h"
#include "utils.h"
#include "DateTime.h"
#include "p8-platform/util/StringUtils.h"

#include <kodi/General.h>
#include <kodi/ActionIDs.h>

/* Dialog item identifiers */
#define BUTTON_OK                       1
#define BUTTON_CANCEL                   2

#define SPIN_CONTROL_FREQUENCY         10
#define SPIN_CONTROL_AIRTIME           11
#define SPIN_CONTROL_CHANNELS          12
#define SPIN_CONTROL_KEEP              13
#define SPIN_CONTROL_PRERECORD         14
#define SPIN_CONTROL_POSTRECORD        15

#define LABEL_PROGRAM_TITLE            20
#define LABEL_PROGRAM_START_TIME       21
#define LABEL_PROGRAM_CHANNEL          22

using namespace MPTV;

CGUIDialogRecordSettings::CGUIDialogRecordSettings(const kodi::addon::PVRTimer& timerinfo, cTimer& timer, const std::string& channelName) :
  kodi::gui::CWindow("DialogRecordSettings.xml", "skin.fallback", true, true),
	m_frequency(Once),
  m_airtime(ThisTime),
  m_channels(ThisChannel),
  m_timerinfo(timerinfo),
  m_timer(timer)
{
  CDateTime startTime(m_timerinfo.GetStartTime());
  CDateTime endTime(m_timerinfo.GetEndTime());
  startTime.GetAsLocalizedTime(m_startTime);
  startTime.GetAsLocalizedDate(m_startDate);
  endTime.GetAsLocalizedTime(m_endTime);

  m_title = m_timerinfo.GetTitle();
  m_channel = channelName;

  // needed for every dialog
  m_retVal = -1; // init to failed load value (due to xml file not being found)
}

bool CGUIDialogRecordSettings::OnInit()
{
  m_spinFrequency = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_FREQUENCY);
  m_spinAirtime = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_AIRTIME);
  m_spinChannels = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_CHANNELS);
  m_spinKeep = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_KEEP);
  m_spinPreRecord = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_PRERECORD);
  m_spinPostRecord = std::make_shared<kodi::gui::controls::CSpin>(this, SPIN_CONTROL_POSTRECORD);

  // Display the recording details in the window
  SetControlLabel(LABEL_PROGRAM_TITLE, m_title);
  std::string strTimeSlot = m_startDate + " " + m_startTime + " - " + m_endTime;
  SetControlLabel(LABEL_PROGRAM_START_TIME, strTimeSlot);
  SetControlLabel(LABEL_PROGRAM_CHANNEL, m_channel);

  // Populate Frequency spin control
  m_spinFrequency->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  for (int i = 0; i < 5; i++)
  { // show localized recording options
    m_spinFrequency->AddLabel(kodi::GetLocalizedString(30110 + i), i);
  }
  // set the default value
  m_spinFrequency->SetIntValue(CGUIDialogRecordSettings::Once);

  // Populate Airtime spin control
  std::string strThisTime = kodi::GetLocalizedString(30120);
  strThisTime += "(" + m_startTime + ")";
  m_spinAirtime->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  m_spinAirtime->AddLabel(strThisTime, CGUIDialogRecordSettings::ThisTime);
  m_spinAirtime->AddLabel(kodi::GetLocalizedString(30121), CGUIDialogRecordSettings::AnyTime);
  // Set the default values
  m_spinAirtime->SetIntValue(CGUIDialogRecordSettings::ThisTime);
  m_spinAirtime->SetVisible(false);

  // Populate Channels spin control
  m_spinChannels->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  for (int i = 0; i < 2; i++)
  { // show localized recording options
    m_spinChannels->AddLabel(kodi::GetLocalizedString(30125 + i), i);
  }
  // Set the default values
  m_spinChannels->SetIntValue(CGUIDialogRecordSettings::ThisChannel);
  m_spinChannels->SetVisible(false);

  // Populate Keep spin control
  m_spinKeep->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  for (int i = 0; i < 4; i++)
  { // show localized recording options
    m_spinKeep->AddLabel(kodi::GetLocalizedString(30130 + i), i);
  }
  // Set the default values
  m_spinKeep->SetIntValue(TvDatabase::Always);

  // Populate PreRecord spin control
  std::string marginStart;
  marginStart = StringUtils::Format("%d (%s)", m_timerinfo.GetMarginStart(), kodi::GetLocalizedString(30136).c_str());
  m_spinPreRecord->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  m_spinPreRecord->AddLabel(kodi::GetLocalizedString(30135), -1);
  m_spinPreRecord->AddLabel(marginStart, m_timerinfo.GetMarginStart()); //value from XBMC
  m_spinPreRecord->AddLabel("0", 0);
  m_spinPreRecord->AddLabel("3", 3);
  m_spinPreRecord->AddLabel("5", 5);
  m_spinPreRecord->AddLabel("7", 7);
  m_spinPreRecord->AddLabel("10", 10);
  m_spinPreRecord->AddLabel("15", 15);
  m_spinPreRecord->SetIntValue(m_timerinfo.GetMarginStart());  // Set the default value

  // Populate PostRecord spin control
  std::string marginEnd;
  marginEnd = StringUtils::Format("%d (%s)", m_timerinfo.GetMarginEnd(), kodi::GetLocalizedString(30136).c_str());
  m_spinPostRecord->SetType(kodi::gui::controls::ADDON_SPIN_CONTROL_TYPE_TEXT);
  m_spinPostRecord->AddLabel(kodi::GetLocalizedString(30135), -1);
  m_spinPostRecord->AddLabel(marginEnd, m_timerinfo.GetMarginEnd()); //value from XBMC
  m_spinPostRecord->AddLabel("0", 0);
  m_spinPostRecord->AddLabel("3", 3);
  m_spinPostRecord->AddLabel("5", 5);
  m_spinPostRecord->AddLabel("7", 7);
  m_spinPostRecord->AddLabel("10", 10);
  m_spinPostRecord->AddLabel("15", 15);
  m_spinPostRecord->AddLabel("20", 20);
  m_spinPostRecord->AddLabel("30", 30);
  m_spinPostRecord->AddLabel("45", 45);
  m_spinPostRecord->AddLabel("60", 60);
  m_spinPostRecord->SetIntValue(m_timerinfo.GetMarginEnd());   // Set the default value

  return true;
}

bool CGUIDialogRecordSettings::OnClick(int controlId)
{
  switch(controlId)
  {
    case BUTTON_OK:				// save value from GUI, then FALLS THROUGH TO CANCEL
      m_frequency = (RecordingFrequency) m_spinFrequency->GetIntValue();
      m_airtime = (RecordingAirtime) m_spinAirtime->GetIntValue();
      m_channels = (RecordingChannels) m_spinChannels->GetIntValue();

      /* Update the Timer settings */
      UpdateTimerSettings();

      m_retVal = 1;
      Close();
      break;
    case BUTTON_CANCEL:
      m_retVal = 0;
      Close();
      break;
    /* Limit the available options based on the SPIN settings
    * MediaPortal does not support all combinations
    */
    case SPIN_CONTROL_FREQUENCY:
      m_frequency = (RecordingFrequency) m_spinFrequency->GetIntValue();

      switch (m_frequency)
      {
        case Once:
        case Weekends:
        case WeekDays:
          m_spinAirtime->SetVisible(false);
          m_spinChannels->SetVisible(false);
          break;
        case Weekly:
          m_spinAirtime->SetVisible(true);
          m_spinChannels->SetVisible(false);
          break;
        case Daily:
          m_spinAirtime->SetVisible(true);
          m_spinChannels->SetVisible(true);
          break;
      }
      break;
    case SPIN_CONTROL_CHANNELS:
      m_channels = (RecordingChannels) m_spinChannels->GetIntValue();

      //switch (m_frequency)
      //{
      //  case Once:
      //  case Weekends:
      //  case WeekDays:
      //  case Weekly:
      //    m_channels = ThisChannel;
      //    m_spinChannels->SetValue(m_channels);
      //    break;
      //}

      /* This time on any channel is not supported by MediaPortal */
      if (m_channels == AnyChannel)
        m_spinAirtime->SetIntValue(AnyTime);
      break;
    case SPIN_CONTROL_AIRTIME:
      m_airtime = (RecordingAirtime) m_spinAirtime->GetIntValue();

      if (m_airtime == ThisTime)
        m_spinChannels->SetIntValue(ThisChannel);
      break;
  }

  return true;
}

bool CGUIDialogRecordSettings::Show()
{
  kodi::gui::CWindow::Show();
  return false;
}

void CGUIDialogRecordSettings::Close()
{
  kodi::gui::CWindow::Close();
}

int CGUIDialogRecordSettings::DoModal()
{
  kodi::gui::CWindow::DoModal();
  return m_retVal;
}

bool CGUIDialogRecordSettings::OnFocus(int controlId)
{
  return true;
}

/*
 * This callback is called by XBMC before executing its internal OnAction() function
 * Returning "true" tells XBMC that we already handled the action, returing "false"
 * passes action to the XBMC internal OnAction() function
 */
bool CGUIDialogRecordSettings::OnAction(int actionId, uint32_t buttoncode, wchar_t unicode)
{
  //kodi::Log(ADDON_ADDON::LOG_DEBUG, "%s: action = %i\n", __FUNCTION__, actionId);
  if (actionId == ACTION_PREVIOUS_MENU || actionId == ACTION_NAV_BACK)
    return OnClick(BUTTON_CANCEL);
  else
    /* return false to tell XBMC that it should take over */
    return false;
}

void CGUIDialogRecordSettings::UpdateTimerSettings(void)
{
  switch(m_frequency)
  {
    case Once:
      m_timer.SetScheduleRecordingType(TvDatabase::Once);
      break;
    case Weekends:
      m_timer.SetScheduleRecordingType(TvDatabase::Weekends);
      break;
    case WeekDays:
      m_timer.SetScheduleRecordingType(TvDatabase::WorkingDays);
      break;
    case Weekly:
      if (m_airtime == ThisTime)
        m_timer.SetScheduleRecordingType(TvDatabase::Weekly);
      else
        m_timer.SetScheduleRecordingType(TvDatabase::WeeklyEveryTimeOnThisChannel);
      break;
    case Daily:
      switch (m_airtime)
      {
        case ThisTime:
          m_timer.SetScheduleRecordingType(TvDatabase::Daily);
          break;
        case AnyTime:
          if (m_channels == ThisChannel)
            m_timer.SetScheduleRecordingType(TvDatabase::EveryTimeOnThisChannel);
          else
            m_timer.SetScheduleRecordingType(TvDatabase::EveryTimeOnEveryChannel);
          break;
      }
  }

  m_timer.SetKeepMethod((TvDatabase::KeepMethodType)  m_spinKeep->GetIntValue());
  m_timer.SetPreRecordInterval(m_spinPreRecord->GetIntValue());
  m_timer.SetPostRecordInterval(m_spinPostRecord->GetIntValue());
}
