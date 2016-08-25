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

#include "p8-platform/util/timeutils.h"
#include "p8-platform/threads/mutex.h"
#include "p8-platform/util/util.h"
#include "MemoryBuffer.h"
#include "client.h"
#include "TSDebug.h"

using namespace ADDON;

#define MAX_MEMORY_BUFFER_SIZE (1024L*1024L*12L)

CMemoryBuffer::CMemoryBuffer(void)
{
  m_bRunning = true;
  m_BytesInBuffer = 0;
}

CMemoryBuffer::~CMemoryBuffer()
{
  Clear();
}

bool CMemoryBuffer::IsRunning()
{
  return m_bRunning;
}

void CMemoryBuffer::Clear()
{
	P8PLATFORM::CLockObject BufferLock(m_BufferLock);
  std::vector<BufferItem *>::iterator it = m_Array.begin();

  for ( ; it != m_Array.end(); ++it )
  {
    BufferItem *item = *it;
    SAFE_DELETE_ARRAY(item->data);
    SAFE_DELETE(item);
  }

  m_Array.clear();
  m_BytesInBuffer = 0;
}

size_t CMemoryBuffer::Size()
{
  return m_BytesInBuffer;
}

void CMemoryBuffer::Run(bool onOff)
{
  TSDEBUG(LOG_DEBUG, "memorybuffer: run:%d %d", onOff, m_bRunning);

  if (m_bRunning != onOff)
  {
    m_bRunning = onOff;

    if (m_bRunning == false)
    {
      Clear();
    }
  }

  TSDEBUG(LOG_DEBUG, "memorybuffer: running:%d", onOff);
}

size_t CMemoryBuffer::ReadFromBuffer(unsigned char *pbData, size_t lDataLength)
{
  if (pbData == NULL || lDataLength <= 0 || !m_bRunning)
    return 0;

  while (m_BytesInBuffer < lDataLength)
  {
    if (!m_bRunning)
      return 0;
    m_event.Wait(5000);
    if (!m_bRunning)
      return 0;
  }

  // XBMC->Log(LOG_DEBUG, "get..%d/%d", lDataLength, m_BytesInBuffer);
  size_t bytesWritten = 0;
	P8PLATFORM::CLockObject BufferLock(m_BufferLock);

  while (bytesWritten < lDataLength)
  {
    if (m_Array.empty())
    {
      XBMC->Log(LOG_DEBUG, "memorybuffer: read:empty buffer\n");
      return 0;
    }
    BufferItem *item = m_Array.at(0);

    if (NULL == item)
    {
      XBMC->Log(LOG_DEBUG, "memorybuffer: item==NULL\n");
      return 0;
    }

    size_t copyLength;
    if ( (item->nDataLength - item->nOffset) < (lDataLength - bytesWritten) )
    {
      copyLength = item->nDataLength - item->nOffset;
    }
    else
    {
      copyLength = lDataLength - bytesWritten;
    }

    if (NULL == item->data)
    {
      XBMC->Log(LOG_DEBUG, "memorybuffer: item->data==NULL\n");
      return 0;
    }

    memcpy(&pbData[bytesWritten], &item->data[item->nOffset], copyLength);

    bytesWritten += copyLength;
    item->nOffset += copyLength;
    m_BytesInBuffer -= copyLength;

    if (item->nOffset >= item->nDataLength)
    {
      m_Array.erase(m_Array.begin());
      SAFE_DELETE_ARRAY(item->data);
      SAFE_DELETE(item);
    }
  }
  return bytesWritten;
}

long CMemoryBuffer::PutBuffer(unsigned char *pbData, size_t lDataLength)
{
  if (lDataLength <= 0 || pbData == NULL) return E_FAIL;

  BufferItem* item = new BufferItem();
  item->nOffset = 0;
  item->nDataLength = lDataLength;
  item->data = new byte[lDataLength];
  memcpy(item->data, pbData, lDataLength);
  bool sleep = false;
  {
		P8PLATFORM::CLockObject BufferLock(m_BufferLock);
    m_Array.push_back(item);
    m_BytesInBuffer += lDataLength;

    //XBMC->Log(LOG_DEBUG, "add..%d/%d",lDataLength,m_BytesInBuffer);
    while (m_BytesInBuffer > MAX_MEMORY_BUFFER_SIZE)
    {
      sleep = true;
      XBMC->Log(LOG_DEBUG, "memorybuffer:put full buffer (%d)", (unsigned long) m_BytesInBuffer);
      BufferItem *item2 = m_Array.at(0);
      size_t copyLength = item2->nDataLength - item2->nOffset;

      m_BytesInBuffer -= copyLength;
      m_Array.erase(m_Array.begin());
      SAFE_DELETE_ARRAY(item2->data);
      SAFE_DELETE(item2);
    }
    if (m_BytesInBuffer > 0)
    {
      m_event.Broadcast();
    }
  }

  if (sleep)
  {
    usleep(10000);
  }
  return S_OK;
}

#endif //LIVE555
