#pragma once
/*
 *      Copyright (C) 2005-2011 Team Kodi
 *      https://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <vector>

/* Master defines for client control */
#include "kodi/xbmc_pvr_types.h"

/* Local includes */
#include "Socket.h"
#include "Cards.h"
#include "epg.h"
#include "channels.h"
#include "p8-platform/threads/mutex.h"
#include "p8-platform/threads/threads.h"

/* Use a forward declaration here. Including RTSPClient.h via TSReader.h at this point gives compile errors */
namespace MPTV
{
    class CTsReader;
}
class cRecording;

class cPVRClientMediaPortal: public P8PLATFORM::PreventCopy, public P8PLATFORM::CThread
{
public:
  /* Class interface */
  cPVRClientMediaPortal();
  ~cPVRClientMediaPortal();

  /* Server handling */
  ADDON_STATUS TryConnect();
  void Disconnect();
  bool IsUp();

  /* General handling */
  const char* GetBackendName(void);
  const char* GetBackendVersion(void);
  const char* GetConnectionString(void);
  PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed);
  PVR_ERROR GetBackendTime(time_t *localTime, int *gmtOffset);

  /* EPG handling */
  PVR_ERROR GetEpg(ADDON_HANDLE handle, int iChannelUid, time_t iStart = 0, time_t iEnd = 0);

  /* Channel handling */
  int GetNumChannels(void);
  PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);

  /* Channel group handling */
  int GetChannelGroupsAmount(void);
  PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
  PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

  /* Record handling **/
  int GetNumRecordings(void);
  PVR_ERROR GetRecordings(ADDON_HANDLE handle);
  PVR_ERROR DeleteRecording(const PVR_RECORDING &recording);
  PVR_ERROR RenameRecording(const PVR_RECORDING &recording);
  PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count);
  PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition);
  int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);

  /* Timer handling */
  int GetNumTimers(void);
  PVR_ERROR GetTimers(ADDON_HANDLE handle);
  PVR_ERROR GetTimerInfo(unsigned int timernumber, PVR_TIMER &timer);
  PVR_ERROR AddTimer(const PVR_TIMER &timer);
  PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete = false);
  PVR_ERROR UpdateTimer(const PVR_TIMER &timer);
  PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size);

  /* Live stream handling */
  bool OpenLiveStream(const PVR_CHANNEL &channel);
  void CloseLiveStream();
  int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize);
  PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus);
  PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount);
  long long SeekLiveStream(long long iPosition, int iWhence = SEEK_SET);
  long long LengthLiveStream(void);

  /* Record stream handling */
  bool OpenRecordedStream(const PVR_RECORDING &recording);
  void CloseRecordedStream(void);
  int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize);
  long long SeekRecordedStream(long long iPosition, int iWhence = SEEK_SET);
  long long LengthRecordedStream(void);
  PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING* recording, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount);

  /* Common stream handing functions */
  bool CanPauseAndSeek(void);
  void PauseStream(bool bPaused);
  bool IsRealTimeStream(void);
  PVR_ERROR GetStreamReadChunkSize(int* chunksize);
  PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES* stream_times);

protected:
  MPTV::Socket           *m_tcpclient;

private:
  /* TVServerKodi Listening Thread */
  void* Process(void);
  PVR_CONNECTION_STATE Connect(bool updateConnectionState = true);

  void LoadGenreTable(void);
  void LoadCardSettings(void);
  void SetConnectionState(PVR_CONNECTION_STATE newState);
  cRecording* GetRecordingInfo(const PVR_RECORDING& recording);
  const char* GetConnectionStateString(PVR_CONNECTION_STATE state) const;
  void AddStreamProperty(PVR_NAMED_VALUE* properties, unsigned int* propertiesCount, std::string name, std::string value);

  int                     m_iCurrentChannel;
  int                     m_iCurrentCard;
  bool                    m_bCurrentChannelIsRadio;
  PVR_CONNECTION_STATE    m_state;
  bool                    m_bStop;
  bool                    m_bTimeShiftStarted;
  bool                    m_bSkipCloseLiveStream;
  std::string             m_ConnectionString;
  std::string             m_PlaybackURL;
  std::string             m_BackendName;
  std::string             m_BackendVersion;
  int                     m_BackendUTCoffset;
  time_t                  m_BackendTime;
  CCards                  m_cCards;
  CGenreTable*            m_genretable;
  P8PLATFORM::CMutex      m_mutex;
  P8PLATFORM::CMutex      m_connectionMutex;
  int64_t                 m_iLastRecordingUpdate;
  MPTV::CTsReader*        m_tsreader;
  std::map<int,cChannel>  m_channels;
  int                     m_signalStateCounter;
  int                     m_iSignal;
  int                     m_iSNR;

  cRecording*             m_lastSelectedRecording;

  //Used for TV Server communication:
  std::string SendCommand(const char* command);
  std::string SendCommand(const std::string& command);
  bool SendCommand2(const std::string& command, std::vector<std::string>& lines);
};
