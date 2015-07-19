project(pvr.mediaportal.tvserver)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(platform REQUIRED)
find_package(TinyXML REQUIRED)

# Grab the addon version info from the addon.xml file
FILE(READ ${PROJECT_NAME}/addon.xml.in MPTV_ADDONXML)
STRING(REGEX MATCH "<addon[^>]*version.?=.?.[0-9\\.]+" MPTV_VERSION_STRING ${MPTV_ADDONXML})
STRING(REGEX REPLACE ".*version=.([0-9\\.]+).*" "\\1" MPTV_VERSION ${MPTV_VERSION_STRING})

# Generate an addon_version.h header with the version information
configure_file(src/addon_version.h.in addon_version.h)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${platform_INCLUDE_DIRS}
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

# Make sure that CMake adds the headers to the MSVC project
list(APPEND MPTV_SOURCES ${MPTV_HEADERS} ${RESOURCE_FILES})

add_subdirectory(src/lib/tsreader)

set(DEPLIBS ${platform_LIBRARIES}
            ${TINYXML_LIBRARIES} tsreader)

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif(WIN32)

build_addon(pvr.mediaportal.tvserver MPTV DEPLIBS)

include(CPack)
