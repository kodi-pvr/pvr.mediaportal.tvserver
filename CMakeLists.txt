project(pvr.mediaportal.tvserver)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(TinyXML REQUIRED)

# Grab the addon version info from the addon.xml file
FILE(READ ${PROJECT_NAME}/addon.xml.in MPTV_ADDONXML)
STRING(REGEX MATCH "<addon[^>]*version.?=.?.[0-9\\.]+" MPTV_VERSION_STRING ${MPTV_ADDONXML})
STRING(REGEX REPLACE ".*version=.([0-9\\.]+).*" "\\1" MPTV_VERSION ${MPTV_VERSION_STRING})

# Generate an addon_version.h header with the version information
configure_file(src/addon_version.h.in addon_version.h)

set(LIVE555_INCLUDE_DIR
  src/lib/live555/liveMedia/include
  src/lib/live555/BasicUsageEnvironment/include
  src/lib/live555/UsageEnvironment/include
  src/lib/live555/groupsock/include
  src/lib/live555/
)

set(LIVE555_DEFINES -DLIVE555 -DSOCKLEN_T=socklen_t -DBSD=1)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${TINYXML_INCLUDE_DIR}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src
                    ${PROJECT_BINARY_DIR}
                    ${LIVE555_INCLUDE_DIR}
)

add_definitions(-D__STDC_FORMAT_MACROS ${LIVE555_DEFINES})

if (NOT WIN32)
  add_options(ALL_LANGUAGES ALL_BUILDS "-fPIC")
endif()

# Source files
set(MPTV_SOURCES src/Cards.cpp
                 src/channels.cpp
                 src/client.cpp
                 src/DateTime.cpp
                 src/epg.cpp
                 src/GenreTable.cpp
                 src/GUIDialogRecordSettings.cpp
                 src/pvrclient-mediaportal.cpp
                 src/recordings.cpp
                 src/Socket.cpp
                 src/timers.cpp
                 src/uri.cpp
                 src/utils.cpp)

if(WIN32)
  set(MPTV_SOURCES_WINDOWS src/windows/FileUtils.cpp
                           src/windows/WindowsUtils.cpp)
  source_group("Source Files\\windows" FILES ${MPTV_SOURCES_WINDOWS})
  list(APPEND MPTV_SOURCES ${MPTV_SOURCES_WINDOWS})
endif(WIN32)

# Header files
set(MPTV_HEADERS addon_version.h
                 src/Cards.h
                 src/channels.h
                 src/client.h
                 src/DateTime.h
                 src/epg.h
                 src/GenreTable.h
                 src/GUIDialogRecordSettings.h
                 src/os-dependent.h
                 src/pvrclient-mediaportal.h
                 src/recordings.h
                 src/Socket.h
                 src/timers.h
                 src/uri.h
                 src/utils.h)
source_group("Header Files" FILES ${MPTV_HEADERS})
if(WIN32)
  set(MPTV_HEADERS_WINDOWS src/windows/WindowsUtils.h)
  source_group("Header Files\\windows" FILES ${MPTV_HEADERS_WINDOWS})
  list(APPEND MPTV_HEADERS ${MPTV_HEADERS_WINDOWS})
endif(WIN32)

if(WIN32)
# Misc files
set(RESOURCE_FILES pvr.mediaportal.tvserver/addon.xml
                pvr.mediaportal.tvserver/changelog.txt
                pvr.mediaportal.tvserver/resources/genre_translation.xml
                pvr.mediaportal.tvserver/resources/settings.xml
                pvr.mediaportal.tvserver/resources/language/resource.language.en_gb/strings.po)
source_group("Resource Files" FILES ${RESOURCE_FILES})
endif(WIN32)

# TSReader sources
set(TSREADER_SOURCES src/lib/tsreader/ChannelInfo.cpp
            src/lib/tsreader/DeMultiplexer.cpp
            src/lib/tsreader/DvbUtil.cpp
            src/lib/tsreader/FileReader.cpp
            src/lib/tsreader/MemoryBuffer.cpp
            src/lib/tsreader/MemoryReader.cpp
            src/lib/tsreader/MemorySink.cpp
            src/lib/tsreader/MepoRTSPClient.cpp
            src/lib/tsreader/MultiFileReader.cpp
            src/lib/tsreader/PacketSync.cpp
            src/lib/tsreader/PatParser.cpp
            src/lib/tsreader/PidTable.cpp
            src/lib/tsreader/PmtParser.cpp
            src/lib/tsreader/Section.cpp
            src/lib/tsreader/SectionDecoder.cpp
            src/lib/tsreader/TSHeader.cpp
            src/lib/tsreader/TSReader.cpp
)
source_group("Source Files\\lib\\tsreader" FILES ${TSREADER_SOURCES})

# TSReader sources
set(TSREADER_HEADERS src/lib/tsreader/ChannelInfo.h
            src/lib/tsreader/DeMultiplexer.h
            src/lib/tsreader/DvbUtil.h
            src/lib/tsreader/FileReader.h
            src/lib/tsreader/MemoryBuffer.h
            src/lib/tsreader/MemoryReader.h
            src/lib/tsreader/MemorySink.h
            src/lib/tsreader/MepoRTSPClient.h
            src/lib/tsreader/MultiFileReader.h
            src/lib/tsreader/PacketSync.h
            src/lib/tsreader/PatParser.h
            src/lib/tsreader/PidTable.h
            src/lib/tsreader/PmtParser.h
            src/lib/tsreader/Section.h
            src/lib/tsreader/SectionDecoder.h
            src/lib/tsreader/TSDebug.h
            src/lib/tsreader/TSHeader.h
            src/lib/tsreader/TSReader.h)
source_group("Header Files\\lib\\tsreader" FILES ${TSREADER_HEADERS})

#Live555 subset sources
SET(LIVE555_SOURCES
            src/lib/live555/BasicUsageEnvironment/BasicHashTable.cpp
            src/lib/live555/BasicUsageEnvironment/BasicTaskScheduler.cpp
            src/lib/live555/BasicUsageEnvironment/BasicTaskScheduler0.cpp
            src/lib/live555/BasicUsageEnvironment/BasicUsageEnvironment.cpp
            src/lib/live555/BasicUsageEnvironment/BasicUsageEnvironment0.cpp
            src/lib/live555/BasicUsageEnvironment/DelayQueue.cpp
            src/lib/live555/UsageEnvironment/HashTable.cpp
            src/lib/live555/UsageEnvironment/UsageEnvironment.cpp
            src/lib/live555/UsageEnvironment/strDup.cpp
            src/lib/live555/groupsock/GroupEId.cpp
            src/lib/live555/groupsock/Groupsock.cpp
            src/lib/live555/groupsock/GroupsockHelper.cpp
            src/lib/live555/groupsock/IOHandlers.cpp
            src/lib/live555/groupsock/NetAddress.cpp
            src/lib/live555/groupsock/NetInterface.cpp
            src/lib/live555/groupsock/inet.c
            src/lib/live555/liveMedia/Base64.cpp
            src/lib/live555/liveMedia/BasicUDPSource.cpp
            src/lib/live555/liveMedia/DigestAuthentication.cpp
            src/lib/live555/liveMedia/FramedFilter.cpp
            src/lib/live555/liveMedia/FramedSource.cpp
            src/lib/live555/liveMedia/Locale.cpp
            src/lib/live555/liveMedia/MPEG2TransportStreamFramer.cpp
            src/lib/live555/liveMedia/Media.cpp
            src/lib/live555/liveMedia/MediaSession.cpp
            src/lib/live555/liveMedia/MediaSink.cpp
            src/lib/live555/liveMedia/MediaSource.cpp
            src/lib/live555/liveMedia/MultiFramedRTPSink.cpp
            src/lib/live555/liveMedia/MultiFramedRTPSource.cpp
            src/lib/live555/liveMedia/RTCP.cpp
            src/lib/live555/liveMedia/RTPInterface.cpp
            src/lib/live555/liveMedia/RTPSink.cpp
            src/lib/live555/liveMedia/RTPSource.cpp
            src/lib/live555/liveMedia/RTSPClient.cpp
            src/lib/live555/liveMedia/RTSPCommon.cpp
            src/lib/live555/liveMedia/SimpleRTPSink.cpp
            src/lib/live555/liveMedia/SimpleRTPSource.cpp
            src/lib/live555/liveMedia/our_md5.c
            src/lib/live555/liveMedia/our_md5hl.c
            src/lib/live555/liveMedia/rtcp_from_spec.c
)
source_group("Source Files\\lib\\live555" FILES ${LIVE555_SOURCES})

SET(LIVE555_HEADERS
            src/lib/live555/groupsock/include/NetCommon.h
            src/lib/live555/liveMedia/our_md5.h
            src/lib/live555/liveMedia/rtcp_from_spec.h
            src/lib/live555/BasicUsageEnvironment/include/BasicHashTable.hh
            src/lib/live555/BasicUsageEnvironment/include/BasicUsageEnvironment.hh
            src/lib/live555/BasicUsageEnvironment/include/BasicUsageEnvironment0.hh
            src/lib/live555/BasicUsageEnvironment/include/BasicUsageEnvironment_version.hh
            src/lib/live555/BasicUsageEnvironment/include/DelayQueue.hh
            src/lib/live555/BasicUsageEnvironment/include/HandlerSet.hh
            src/lib/live555/groupsock/include/GroupEId.hh
            src/lib/live555/groupsock/include/Groupsock.hh
            src/lib/live555/groupsock/include/GroupsockHelper.hh
            src/lib/live555/groupsock/include/groupsock_version.hh
            src/lib/live555/groupsock/include/IOHandlers.hh
            src/lib/live555/groupsock/include/NetAddress.hh
            src/lib/live555/groupsock/include/NetInterface.hh
            src/lib/live555/groupsock/include/TunnelEncaps.hh
            src/lib/live555/liveMedia/include/Base64.hh
            src/lib/live555/liveMedia/include/BasicUDPSource.hh
            src/lib/live555/liveMedia/include/DigestAuthentication.hh
            src/lib/live555/liveMedia/include/FramedFilter.hh
            src/lib/live555/liveMedia/include/FramedSource.hh
            src/lib/live555/liveMedia/include/liveMedia.hh
            src/lib/live555/liveMedia/include/liveMedia_version.hh
            src/lib/live555/liveMedia/include/Locale.hh
            src/lib/live555/liveMedia/include/Media.hh
            src/lib/live555/liveMedia/include/MediaSession.hh
            src/lib/live555/liveMedia/include/MediaSink.hh
            src/lib/live555/liveMedia/include/MediaSource.hh
            src/lib/live555/liveMedia/include/MPEG2TransportStreamFramer.hh
            src/lib/live555/liveMedia/include/MultiFramedRTPSink.hh
            src/lib/live555/liveMedia/include/MultiFramedRTPSource.hh
            src/lib/live555/liveMedia/include/RTCP.hh
            src/lib/live555/liveMedia/include/RTPInterface.hh
            src/lib/live555/liveMedia/include/RTPSink.hh
            src/lib/live555/liveMedia/include/RTPSource.hh
            src/lib/live555/liveMedia/include/RTSPClient.hh
            src/lib/live555/liveMedia/include/RTSPCommon.hh
            src/lib/live555/liveMedia/include/SimpleRTPSink.hh
            src/lib/live555/liveMedia/include/SimpleRTPSource.hh
            src/lib/live555/UsageEnvironment/include/Boolean.hh
            src/lib/live555/UsageEnvironment/include/HashTable.hh
            src/lib/live555/UsageEnvironment/include/strDup.hh
            src/lib/live555/UsageEnvironment/include/UsageEnvironment.hh
            src/lib/live555/UsageEnvironment/include/UsageEnvironment_version.hh
)
source_group("Header Files\\lib\\live555" FILES ${LIVE555_HEADERS})

# Make sure that CMake adds all files to the MSVC project
list(APPEND MPTV_SOURCES ${MPTV_HEADERS} ${TSREADER_SOURCES} ${TSREADER_HEADERS} ${RESOURCE_FILES} ${LIVE555_SOURCES} ${LIVE555_HEADERS})

set(DEPLIBS ${p8-platform_LIBRARIES}
            ${TINYXML_LIBRARIES})

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif(WIN32)

build_addon(pvr.mediaportal.tvserver MPTV DEPLIBS)

include(CPack)
