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

#ifdef LIVE555

#include "p8-platform/threads/mutex.h"
#include <vector>

class CMemoryBuffer
{
  public:
    CMemoryBuffer(void);
    virtual ~CMemoryBuffer(void);

    size_t ReadFromBuffer(unsigned char *pbData, size_t lDataLength);
    long PutBuffer(unsigned char *pbData, size_t lDataLength);
    void Clear();
    size_t Size();
    void Run(bool onOff);
    bool IsRunning();

    typedef struct
    {
      unsigned char* data;
      size_t   nDataLength;
      size_t   nOffset;
    } BufferItem;

  protected:
    std::vector<BufferItem *> m_Array;
    P8PLATFORM::CMutex m_BufferLock;
    size_t    m_BytesInBuffer;
		P8PLATFORM::CEvent m_event;
    bool m_bRunning;
};
#endif //LIVE555
