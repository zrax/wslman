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

cmake_minimum_required(VERSION 3.10)
project(wslman)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 COMPONENTS Core Widgets)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets WinExtras)
endif()
set(CMAKE_AUTORCC ON)

find_package(LibArchive REQUIRED)
option(LibArchive_STATIC "Use LibArchive built as a static library" ON)

if(LibArchive_STATIC)
    find_package(ZLIB REQUIRED)
    set(LibArchive_LIBRARIES ${LibArchive_LIBRARIES} ${ZLIB_LIBRARIES})
    find_package(BZip2 REQUIRED)
    set(LibArchive_LIBRARIES ${LibArchive_LIBRARIES} ${BZIP2_LIBRARIES})
    find_package(LibLZMA REQUIRED)
    set(LibArchive_LIBRARIES ${LibArchive_LIBRARIES} ${LIBLZMA_LIBRARIES})
endif()

add_executable(wslman WIN32 wslman.cpp)
target_sources(wslman PRIVATE
    wslfs.h
    wslfs.cpp
    wslinstall.h
    wslinstall.cpp
    wslregistry.h
    wslregistry.cpp
    wslsetuser.h
    wslsetuser.cpp
    wslwrap.h
    wslwrap.cpp
    wslui.h
    wslui.cpp
    wslutils.h
    wslutils.cpp
    wslman.qrc
    wslman.rc
    wslman.manifest
)

target_include_directories(wslman PRIVATE
    ${LibArchive_INCLUDE_DIRS}
)

if(LibArchive_STATIC)
    target_compile_definitions(wslman PRIVATE LIBARCHIVE_STATIC)
endif()

if(Qt6_FOUND)
    target_link_libraries(wslman PRIVATE
        Qt6::Core
        Qt6::Widgets
    )
else()
    target_link_libraries(wslman PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::WinExtras
    )
endif()
target_link_libraries(wslman PRIVATE
    ${LibArchive_LIBRARIES}
    Ntdll.lib
)

# Find windeployqt, which should be in the same path as QtX::qmake
if(Qt6_FOUND)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    set(_deploy_args)
else()
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    set(_deploy_args --no-angle --no-opengl-sw)
endif()
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt.exe is required for installation")
endif()

install(TARGETS wslman RUNTIME DESTINATION .)
install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"\
    ${_deploy_args} \"${CMAKE_INSTALL_PREFIX}/wslman.exe\"\
)")
