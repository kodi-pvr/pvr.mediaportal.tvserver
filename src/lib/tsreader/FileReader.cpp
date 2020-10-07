/*
 *      Copyright (C) 2005-2012 Team Kodi
 *      https://kodi.tv
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
 *************************************************************************
 *  This file originates from TSFileSource, a GPL directshow push
 *  source filter that provides an MPEG transport stream output.
 *  Copyright (C) 2005-2006 nate, bear
 *  http://forums.dvbowners.com/
 */

#include "FileReader.h"
#include <kodi/General.h> //for kodi::Log
#include "TSDebug.h"
#include "p8-platform/threads/threads.h"
#include <algorithm> //std::min, std::max
#include "p8-platform/util/timeutils.h" // for usleep
#include "utils.h"
#include <errno.h>


/* indicate that caller can handle truncated reads, where function returns before entire buffer has been filled */
#define READ_TRUNCATED 0x01

/* indicate that that caller support read in the minimum defined chunk size, this disables internal cache then */
#define READ_CHUNKED   0x02

/* use cache to access this file */
#define READ_CACHED     0x04

/* open without caching. regardless to file type. */
#define READ_NO_CACHE  0x08

/* calcuate bitrate for file while reading */
#define READ_BITRATE   0x10

template<typename T> void SafeDelete(T*& p)
{
  if (p)
  {
    delete p;
    p = nullptr;
  }
}
namespace MPTV
{
    FileReader::FileReader() :
        m_fileSize(0),
        m_fileName("")
    {
    }

    FileReader::~FileReader()
    {
        CloseFile();
    }


    long FileReader::GetFileName(std::string& fileName)
    {
        fileName = m_fileName;
        return S_OK;
    }


    long FileReader::SetFileName(const std::string& fileName)
    {
        m_fileName = ToKodiPath(fileName);
        return S_OK;
    }

    //
    // OpenFile
    //
    // Opens the file ready for streaming
    //
    long FileReader::OpenFile(const std::string& fileName)
    {
        SetFileName(fileName);
        return OpenFile();
    }

    long FileReader::OpenFile()
    {
        int Tmo = 25; //5 in MediaPortal


        // Is the file already opened
        if (!IsFileInvalid())
        {
            kodi::Log(ADDON_LOG_INFO, "FileReader::OpenFile() file already open");
            return S_OK;
        }

        // Has a filename been set yet
        if (m_fileName.empty())
        {
            kodi::Log(ADDON_LOG_ERROR, "FileReader::OpenFile() no filename");
            return ERROR_INVALID_NAME;
        }

        do
        {
            kodi::Log(ADDON_LOG_INFO, "FileReader::OpenFile() %s.", m_fileName.c_str());
            if (m_hFile.OpenFile(m_fileName, READ_CHUNKED))
            {
                break;
            }
            else
            {
                kodi::vfs::FileStatus buffer;
                bool statResult = kodi::vfs::StatFile(m_fileName, buffer);

                if (!statResult)
                {
                    if (errno == EACCES)
                    {
                        kodi::Log(ADDON_LOG_ERROR, "Permission denied. Check the file or share access rights for '%s'", m_fileName.c_str());
                        kodi::QueueNotification(QUEUE_ERROR, "", "Permission denied");
                        Tmo = 0;
                        break;
                    }
                }
            }
            usleep(20000);
        } while (--Tmo);

        if (Tmo)
        {
            if (Tmo < 4) // 1 failed + 1 succeded is quasi-normal, more is a bit suspicious ( disk drive too slow or problem ? )
                kodi::Log(ADDON_LOG_DEBUG, "FileReader::OpenFile(), %d tries to succeed opening %ws.", 6 - Tmo, m_fileName.c_str());
        }
        else
        {
            kodi::Log(ADDON_LOG_ERROR, "FileReader::OpenFile(), open file %s failed.", m_fileName.c_str());
            return S_FALSE;
        }

        kodi::Log(ADDON_LOG_DEBUG, "%s: OpenFile(%s) succeeded.", __FUNCTION__, m_fileName.c_str());

        SetFilePointer(0, SEEK_SET);

        return S_OK;

    } // Open

    //
    // CloseFile
    //
    // Closes any dump file we have opened
    //
    long FileReader::CloseFile()
    {
        if (m_hFile.IsOpen())
        {
            m_hFile.Close();
        }

        return S_OK;
    } // CloseFile


    inline bool FileReader::IsFileInvalid()
    {
        return !m_hFile.IsOpen();
    }


    int64_t FileReader::SetFilePointer(int64_t llDistanceToMove, unsigned long dwMoveMethod)
    {
        TSDEBUG(ADDON_LOG_DEBUG, "%s: distance %d method %d.", __FUNCTION__, llDistanceToMove, dwMoveMethod);
        int64_t rc = m_hFile.Seek(llDistanceToMove, dwMoveMethod);
        TSDEBUG(ADDON_LOG_DEBUG, "%s: distance %d method %d returns %d.", __FUNCTION__, llDistanceToMove, dwMoveMethod, rc);
        return rc;
    }


    int64_t FileReader::GetFilePointer()
    {
        return m_hFile.GetPosition();
    }


    long FileReader::Read(unsigned char* pbData, size_t lDataLength, size_t *dwReadBytes)
    {
        ssize_t read_bytes = m_hFile.Read((void*)pbData, lDataLength);//Read file data into buffer
        if (read_bytes < 0)
        {
            TSDEBUG(ADDON_LOG_DEBUG, "%s: ReadFile function failed.", __FUNCTION__);
            *dwReadBytes = 0;
            return S_FALSE;
        }
        *dwReadBytes = static_cast<size_t>(read_bytes);
        TSDEBUG(ADDON_LOG_DEBUG, "%s: requested read length %d actually read %d.", __FUNCTION__, lDataLength, *dwReadBytes);

        if (*dwReadBytes < lDataLength)
        {
            kodi::Log(ADDON_LOG_INFO, "%s: requested %d bytes, read only %d bytes.", __FUNCTION__, lDataLength, *dwReadBytes);
            return S_FALSE;
        }
        return S_OK;
    }


    int64_t FileReader::GetFileSize()
    {
        return m_hFile.GetLength();
    }

    int64_t FileReader::OnChannelChange(void)
    {
        return m_hFile.GetPosition();
    }
}
