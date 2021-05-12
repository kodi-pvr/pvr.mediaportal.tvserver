#pragma once
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
 */

#ifdef LIVE555

#include "FileReader.h"
#include "MemoryBuffer.h"
#include "os-dependent.h"

namespace MPTV
{
    class CMemoryReader : public FileReader
    {
      public:
        CMemoryReader(CMemoryBuffer& buffer);
        virtual ~CMemoryReader(void);
        virtual long Read(unsigned char* pbData, size_t lDataLength, size_t *dwReadBytes);
        virtual int64_t SetFilePointer(int64_t llDistanceToMove, unsigned long dwMoveMethod);
        virtual bool IsBuffer() { return true; };
        virtual long CloseFile() { return S_OK; };
        virtual size_t HasData();

      private:
        CMemoryBuffer& m_buffer;
        CMemoryReader& operator=(const CMemoryReader& memoryreader) {};
    };
}
#endif //LIVE555
