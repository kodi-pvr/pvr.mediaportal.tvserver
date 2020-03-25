[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](LICENSE.md)
[![Build Status](https://travis-ci.org/kodi-pvr/pvr.mediaportal.tvserver.svg?branch=Matrix)](https://travis-ci.org/kodi-pvr/pvr.mediaportal.tvserver/branches)
[![Build Status](https://dev.azure.com/teamkodi/kodi-pvr/_apis/build/status/kodi-pvr.pvr.mediaportal.tvserver?branchName=Matrix)](https://dev.azure.com/teamkodi/kodi-pvr/_build/latest?definitionId=63&branchName=Matrix)
[![Build Status](https://jenkins.kodi.tv/view/Addons/job/kodi-pvr/job/pvr.mediaportal.tvserver/job/Matrix/badge/icon)](https://jenkins.kodi.tv/blue/organizations/jenkins/kodi-pvr%2Fpvr.mediaportal.tvserver/branches/)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/5120/badge.svg)](https://scan.coverity.com/projects/5120)

# MediaPortal TVServer PVR
MediaPortal TVServer PVR client addon for [Kodi] (https://kodi.tv)

## Build instructions

### Linux

1. `git clone --branch master https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/kodi-pvr/pvr.mediaportal.tvserver.git`
3. `cd pvr.mediaportal.tvserver && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.mediaportal.tvserver -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

##### Useful links

* [Kodi's PVR user support] (https://forum.kodi.tv/forumdisplay.php?fid=171)
* [Kodi's PVR development support] (https://forum.kodi.tv/forumdisplay.php?fid=136)
