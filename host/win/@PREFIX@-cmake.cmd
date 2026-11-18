@echo off
setlocal

rem   Copyright (C) 2024 John Tornblom
rem   Copyright (C) 2026 ItsJokerZz
rem
rem This file is free software; you can redistribute it and/or modify it
rem under the terms of the GNU General Public License as published by
rem the Free Software Foundation; either version 3 of the License, or
rem (at your option) any later version.
rem
rem This program is distributed in the hope that it will be useful, but
rem WITHOUT ANY WARRANTY; without even the implied warranty of
rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
rem General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License
rem along with this program; see the file COPYING. If not see
rem <http://www.gnu.org/licenses/>.

set @PLATFORM_UC@_PAYLOAD_SDK=%~dp0..

set CMAKE_GENERATOR=Ninja
set CMAKE_TOOLCHAIN_FILE=%@PLATFORM_UC@_PAYLOAD_SDK%\toolchain\@PREFIX@.cmake
set CMAKE_MAKE_PROGRAM=%@PLATFORM_UC@_PAYLOAD_SDK%\win\ninja.exe

cmake %*
