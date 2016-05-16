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

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${TINYXML_INCLUDE_DIR}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src
                    ${PROJECT_BINARY_DIR})

add_definitions(-D__STDC_FORMAT_MACROS)

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
            src/lib/tsreader/MemoryReader.cpp
            src/lib/tsreader/MultiFileReader.cpp
            src/lib/tsreader/PacketSync.cpp
            src/lib/tsreader/PatParser.cpp
            src/lib/tsreader/PidTable.cpp
            src/lib/tsreader/PmtParser.cpp
            src/lib/tsreader/Section.cpp
            src/lib/tsreader/SectionDecoder.cpp
            src/lib/tsreader/TSHeader.cpp
            src/lib/tsreader/TSReader.cpp)
source_group("Source Files\\lib\\tsreader" FILES ${TSREADER_SOURCES})

# TSReader sources
set(TSREADER_HEADERS src/lib/tsreader/ChannelInfo.h
            src/lib/tsreader/DeMultiplexer.h
            src/lib/tsreader/DvbUtil.h
            src/lib/tsreader/FileReader.h
            src/lib/tsreader/MemoryReader.h
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

# Make sure that CMake adds all files to the MSVC project
list(APPEND MPTV_SOURCES ${MPTV_HEADERS} ${TSREADER_SOURCES} ${TSREADER_HEADERS} ${RESOURCE_FILES})

set(DEPLIBS ${p8-platform_LIBRARIES}
            ${TINYXML_LIBRARIES})

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif(WIN32)

build_addon(pvr.mediaportal.tvserver MPTV DEPLIBS)

include(CPack)
