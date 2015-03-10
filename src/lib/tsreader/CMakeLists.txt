PROJECT(TSReader)

ENABLE_LANGUAGE(CXX)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

include_directories(../..)

# Source files
SET(SOURCES ChannelInfo.cpp
            DeMultiplexer.cpp
            DvbUtil.cpp
            FileReader.cpp
            MemoryReader.cpp
            MultiFileReader.cpp
            PacketSync.cpp
            PatParser.cpp
            PidTable.cpp
            PmtParser.cpp
            Section.cpp
            SectionDecoder.cpp
            TSHeader.cpp
            TSReader.cpp)

# Header files
SET(HEADERS ChannelInfo.h
            DeMultiplexer.h
            DvbUtil.h
            FileReader.h
            MemoryReader.h
            MultiFileReader.h
            PacketSync.h
            PatParser.h
            PidTable.h
            PmtParser.h
            Section.h
            SectionDecoder.h
            TSDebug.h
            TSHeader.h
            TSReader.h)
SOURCE_GROUP("Header Files" FILES ${HEADERS})

ADD_LIBRARY(tsreader STATIC ${HEADERS} ${SOURCES})
