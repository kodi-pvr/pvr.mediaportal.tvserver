/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <ctime>
#include <string>

namespace MPTV
{

const time_t cUndefinedDate = 946681200;   ///> 01-01-2000 00:00:00 in time_t

class CDateTime
{
public:
  CDateTime();
  CDateTime(const struct tm& time);
  CDateTime(const time_t& time);
  virtual ~CDateTime();

  int GetDay() const;
  int GetMonth() const;
  int GetYear() const;
  int GetHour() const;
  int GetMinute() const;
  int GetSecond() const;
  int GetDayOfWeek() const;
  int GetMinuteOfDay() const;

  time_t GetAsTime(void) const;

  /**
   * @brief Converts the stored datetime value to a string with the date representation for current locale
   * @param strDate     the date string (return value)
   */
  void GetAsLocalizedDate(std::string& strDate) const;

  /**
   * @brief Converts the stored datetime value to a string with the time representation for current locale
   * @param strDate     the date string (return value)
   */
  void GetAsLocalizedTime(std::string& strTime) const;
  
  /**
   * @brief Sets the date and time from a C# DateTime string
   * Assumes the usage of somedatetimeval.ToString("u") in C#
   */
  bool SetFromDateTime(const std::string& dateTime);

  /**
   * @brief Sets the date and time from a time_t value
   */
  void SetFromTime(const time_t& time);
  void SetFromTM(const struct tm& time);

  int operator -(const CDateTime& right) const;
  const CDateTime& operator =(const time_t& right);
  const CDateTime& operator =(const tm& right);
  bool operator ==(const time_t& right) const;
  const CDateTime& operator +=(const int seconds);

  static time_t Now();
private:
  void InitLocale(void);

  struct tm m_time;
};

} // namespace MPTV
