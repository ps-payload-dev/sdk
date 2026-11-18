@echo off
setlocal

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

rem usage: @PREFIX@-setenv.cmd [/r] [/n]
rem export @PLATFORM_UC@_PAYLOAD_SDK for every program of the current user, /r removes it
rem again and /n only shows what would be done. The SDK is the folder above win.

set NAME=@PLATFORM_UC@_PAYLOAD_SDK
for %%I in ("%~dp0..") do set "SDK=%%~fI"
set "REMOVE="
set "DRY="

:args
    if "%~1"=="" goto run
    if /i "%~1"=="/r" (
        set "REMOVE=1"
    ) else if /i "%~1"=="/n" (
        set "DRY=1"
    ) else (
        echo usage: %~nx0 [/r] [/n]
        exit /b 1
    )
    shift
    goto args

:run
if defined REMOVE (
    if defined DRY (
        echo would run: reg delete HKCU\Environment /v %NAME% /f
        exit /b 0
    )
    reg query HKCU\Environment /v %NAME% >nul 2>&1
    if errorlevel 1 (
        echo %NAME% is not set
        exit /b 0
    )
    reg delete HKCU\Environment /v %NAME% /f >nul || exit /b 1
    echo %NAME% is no longer exported, new terminals will not have it
    exit /b 0
)

if defined DRY (
    echo would run: setx %NAME% "%SDK%"
    exit /b 0
)
setx %NAME% "%SDK%" >nul || exit /b 1
echo %NAME% will be %SDK% in new terminals and programs
