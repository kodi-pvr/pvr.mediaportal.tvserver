project(pvr.mediaportal.tvserver)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(TinyXML REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${TINYXML_INCLUDE_DIR}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src)

add_definitions(-D__STDC_FORMAT_MACROS)

if (NOT WIN32)
  add_options(ALL_LANGUAGES ALL_BUILDS "-fPIC")
endif()

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
  list(APPEND MPTV_SOURCES src/windows/FileUtils.cpp
                           src/windows/WindowsUtils.cpp)
endif()

add_subdirectory(src/lib/tsreader)

set(DEPLIBS ${kodiplatform_LIBRARIES}
            ${TINYXML_LIBRARIES} tsreader)

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif()

build_addon(pvr.mediaportal.tvserver MPTV DEPLIBS)

include(CPack)
