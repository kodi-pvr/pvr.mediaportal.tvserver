PROJECT(TSReader)

ENABLE_LANGUAGE(CXX)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

include_directories(../..)

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

ADD_LIBRARY(tsreader STATIC ${SOURCES})
