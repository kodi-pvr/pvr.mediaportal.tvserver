/*
 *      Copyright (C) 2005-2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *************************************************************************
 *  This file is a modified version from Team MediaPortal's
 *  TsReader DirectShow filter
 *  MediaPortal is a GPL'ed HTPC-Application
 *  Copyright (C) 2005-2012 Team MediaPortal
 *  http://www.team-mediaportal.com
 *
 * Changes compared to Team MediaPortal's version:
 * - Code cleanup for PVR addon usage
 * - Code refactoring for cross platform usage
 */

#if defined LIVE555

#include "p8-platform/os.h"
#include "MemorySink.h"
#include "GroupsockHelper.hh"
#include "client.h"
#include "utils.h"

using namespace ADDON;

#define SUBMIT_BUF_SIZE (1316*30)

CMemorySink::CMemorySink(UsageEnvironment& env, CMemoryBuffer& buffer, size_t bufferSize)
  : MediaSink(env),
  fBufferSize(bufferSize),
  m_buffer(buffer)
{
  fBuffer = new byte[bufferSize];
  m_pSubmitBuffer = new byte[SUBMIT_BUF_SIZE];
  m_iSubmitBufferPos = 0;
  m_bReEntrant = false;
}

CMemorySink::~CMemorySink()
{
  delete[] fBuffer;
  delete[] m_pSubmitBuffer;
}

CMemorySink* CMemorySink::createNew(UsageEnvironment& env, CMemoryBuffer& buffer, size_t bufferSize)
{
  return new CMemorySink(env, buffer, bufferSize);
}

Boolean CMemorySink::continuePlaying()
{
  if (fSource == NULL)
    return False;

  fSource->getNextFrame(fBuffer, (unsigned) fBufferSize, afterGettingFrame, this, onSourceClosure, this);
  return True;
}

void CMemorySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned /*numTruncatedBytes*/, struct timeval presentationTime, unsigned /*durationInMicroseconds*/) 
{
  CMemorySink* sink = (CMemorySink*)clientData;
  sink->afterGettingFrame1(frameSize, presentationTime);
  sink->continuePlaying();
}

void CMemorySink::addData(unsigned char* data, size_t dataSize, struct timeval UNUSED(presentationTime))
{
  if (dataSize == 0 || data == NULL) return;

  if (m_bReEntrant)
  {
    XBMC->Log(LOG_DEBUG, "REENTRANT IN MEMORYSINK.CPP");
    return;
  }

	P8PLATFORM::CLockObject BufferLock(m_BufferLock);

  m_bReEntrant = true;
  m_buffer.PutBuffer(data, dataSize);
  m_bReEntrant = false;
}


void CMemorySink::afterGettingFrame1(unsigned frameSize, struct timeval presentationTime)
{
  addData(fBuffer, frameSize, presentationTime);
}
#endif //LIVE555
