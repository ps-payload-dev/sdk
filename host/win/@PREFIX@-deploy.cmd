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

set SCRIPT_PATH=%~dp0
set @PLATFORM_UC@_PORT=9021
set @PLATFORM_UC@_HOST=@PLATFORM@
set "@PLATFORM_UC@_PAYLOAD="

:argparse
if "%1"=="" goto runcmd
if "%1" == "-h" (
  set @PLATFORM_UC@_HOST=%2
  shift
  shift
  goto :argparse
)
if "%1" == "-p" (
  set @PLATFORM_UC@_PORT=%2
  shift
  shift
  goto :argparse
)
set @PLATFORM_UC@_PAYLOAD=%~1
shift
goto :argparse

:runcmd

if "%@PLATFORM_UC@_PAYLOAD%"=="" (
  echo usage: %0 [-h HOST] [-p PORT] PAYLOAD_PATH
  exit /b 1
)

type "%@PLATFORM_UC@_PAYLOAD%" | "%SCRIPT_PATH%plink.exe" -raw -P %@PLATFORM_UC@_PORT% %@PLATFORM_UC@_HOST%
