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

cmake_minimum_required(VERSION 3.1)
project(wslman)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTORCC ON)

add_executable(wslman WIN32 wslman.cpp)
target_sources(wslman PRIVATE
    wslregistry.h
    wslregistry.cpp
    wslwrap.h
    wslwrap.cpp
    wslui.h
    wslui.cpp
    wslman.qrc
)
target_link_libraries(wslman PRIVATE Qt5::Core Qt5::Widgets)
