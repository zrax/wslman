# This file is part of wslman.
#
# wslman is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# wslman is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with wslman.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.4)
project(wslman)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5WinExtras REQUIRED)
set(CMAKE_AUTORCC ON)

add_executable(wslman WIN32 wslman.cpp)
target_sources(wslman PRIVATE
    wslregistry.h
    wslregistry.cpp
    wslsetuser.h
    wslsetuser.cpp
    wslwrap.h
    wslwrap.cpp
    wslui.h
    wslui.cpp
    wslman.qrc
    wslman.rc
    wslman.manifest
)
target_link_libraries(wslman PRIVATE Qt5::Core Qt5::Widgets Qt5::WinExtras)

# Find windeployqt, which should be in the same path as Qt5::qmake
get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt5_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(Qt5_WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt5_bin_dir}")
if(NOT Qt5_WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt.exe is required for installation")
endif()

install(TARGETS wslman RUNTIME DESTINATION .)
install(CODE "execute_process(COMMAND \"${Qt5_WINDEPLOYQT_EXECUTABLE}\"\
    --no-angle --no-opengl-sw \"${CMAKE_INSTALL_PREFIX}/wslman.exe\"\
)")
