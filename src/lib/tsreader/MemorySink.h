#pragma once
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

#ifndef _MEDIA_SINK_HH
  #include "MediaSink.hh" //Live555 header
#endif

#include "MemoryBuffer.h"
#include "p8-platform/threads/mutex.h"

class CMemorySink: public MediaSink
{
  public:
    static CMemorySink* createNew(UsageEnvironment& env, CMemoryBuffer& buffer, size_t bufferSize = 20000);
    void addData(unsigned char* data, size_t dataSize, struct timeval presentationTime);

  protected:
    CMemorySink(UsageEnvironment& env, CMemoryBuffer& buffer, size_t bufferSize = 20000);
    virtual ~CMemorySink(void);

    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds);
    virtual void afterGettingFrame1(unsigned frameSize,struct timeval presentationTime);

    unsigned char* fBuffer;
		size_t fBufferSize;
    CMemoryBuffer& m_buffer;

  private: // redefined virtual functions:
    virtual Boolean continuePlaying();

		P8PLATFORM::CMutex m_BufferLock;
    unsigned char* m_pSubmitBuffer;
    int   m_iSubmitBufferPos;
    bool  m_bReEntrant;
};
#endif //LIVE555
