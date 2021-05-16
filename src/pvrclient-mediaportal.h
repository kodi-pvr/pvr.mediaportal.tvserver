/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

/* Master defines for client control */
#include <kodi/addon-instance/PVR.h>

/* Local includes */
#include "Socket.h"
#include "Cards.h"
#include "epg.h"
#include "channels.h"

/* Use a forward declaration here. Including RTSPClient.h via TSReader.h at this point gives compile errors */
namespace MPTV
{
    class CTsReader;
}
class cRecording;

class ATTRIBUTE_HIDDEN cPVRClientMediaPortal
  : public kodi::addon::CInstancePVRClient
{
public:
  /* Class interface */
  cPVRClientMediaPortal(KODI_HANDLE instance, const std::string& kodiVersion);
  ~cPVRClientMediaPortal() override;

  /* Server handling */
  ADDON_STATUS TryConnect();
  void Disconnect();
  bool IsUp();

  /* General handling */
  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;
  PVR_ERROR GetDriveSpace(uint64_t& total, uint64_t& used) override;
  PVR_ERROR GetBackendTime(time_t *localTime, int *gmtOffset);

  /* EPG handling */
  PVR_ERROR GetEPGForChannel(int channelUid, time_t start, time_t end, kodi::addon::PVREPGTagsResultSet& results) override;

  /* Channel handling */
  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;

  /* Channel group handling */
  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group, kodi::addon::PVRChannelGroupMembersResultSet& results) override;

  /* Record handling **/
  PVR_ERROR GetRecordingsAmount(bool deleted, int& amount) override;
  PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
  PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR RenameRecording(const kodi::addon::PVRRecording& recording) override;
  PVR_ERROR SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count) override;
  PVR_ERROR SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int lastplayedposition) override;
  PVR_ERROR GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position) override;
  PVR_ERROR GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl) override;

  /* Timer handling */
  PVR_ERROR GetTimersAmount(int& amount) override;
  PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
  PVR_ERROR GetTimerInfo(unsigned int timernumber, kodi::addon::PVRTimer& timer);
  PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete) override;
  PVR_ERROR UpdateTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types) override;

  /* Live stream handling */
  bool OpenLiveStream(const kodi::addon::PVRChannel& channel) override;
  void CloseLiveStream() override;
  int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) override;
  PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus) override;
  PVR_ERROR GetChannelStreamProperties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties) override;
  int64_t SeekLiveStream(int64_t iPosition, int iWhence = SEEK_SET) override;
  int64_t LengthLiveStream(void) override;

  /* Record stream handling */
  bool OpenRecordedStream(const kodi::addon::PVRRecording& recording) override;
  void CloseRecordedStream() override;
  int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) override;
  int64_t SeekRecordedStream(int64_t iPosition, int iWhence = SEEK_SET) override;
  int64_t LengthRecordedStream() override;
  PVR_ERROR GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVRStreamProperty>& properties) override;

  /* Common stream handing functions */
  bool CanPauseStream() override;
  bool CanSeekStream() override;
  void PauseStream(bool bPaused) override;
  bool IsRealTimeStream() override;
  PVR_ERROR GetStreamReadChunkSize(int& chunksize) override;
  PVR_ERROR GetStreamTimes(kodi::addon::PVRStreamTimes& stream_times) override;

protected:
  MPTV::Socket           *m_tcpclient;

private:
  /* TVServerKodi Listening Thread */
  void Process();
  PVR_CONNECTION_STATE Connect(bool updateConnectionState = true);

  void LoadGenreTable(void);
  void LoadCardSettings(void);
  void SetConnectionState(PVR_CONNECTION_STATE newState);
  cRecording* GetRecordingInfo(const kodi::addon::PVRRecording& recording);
  const char* GetConnectionStateString(PVR_CONNECTION_STATE state) const;

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
  std::mutex              m_mutex;
  std::mutex              m_connectionMutex;
  std::chrono::system_clock::time_point m_iLastRecordingUpdate;
  MPTV::CTsReader*        m_tsreader;
  std::map<int,cChannel>  m_channels;
  int                     m_signalStateCounter;
  int                     m_iSignal;
  int                     m_iSNR;

  cRecording*             m_lastSelectedRecording;

  std::atomic<bool> m_running = {false};
  std::thread m_thread;

  //Used for TV Server communication:
  std::string SendCommand(const char* command);
  std::string SendCommand(const std::string& command);
  bool SendCommand2(const std::string& command, std::vector<std::string>& lines);
};
