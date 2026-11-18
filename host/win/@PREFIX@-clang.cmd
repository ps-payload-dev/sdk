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
set @PLATFORM_UC@_PAYLOAD_SDK=%SCRIPT_PATH%..

set LIBS_CRT="%@PLATFORM_UC@_PAYLOAD_SDK%\target\lib\crt1.o"
set LIBS_C=-lc
set LIBS_DEPS=-lSceLibcInternal -lSceNet
set LIBS_KERN=-lkernel_web
set "TU="
set ARGS=%*

:checkargs
    if "%~1"=="" goto runcmd
    set "ARG=%~1"
    if "%~1"=="-nostdlibc" (
        set "LIBS_C="
        set "LIBS_DEPS="
	set "LIBS_KERN="
    ) else if "%~1"=="-nodefaultlibs" (
        set "LIBS_C="
        set "LIBS_DEPS="
	set "LIBS_KERN="
    ) else if "%~1"=="-lkernel" (
	set "LIBS_KERN="
    ) else if "%~1"=="-lkernel_sys" (
	set "LIBS_KERN="
    ) else if "%~1"=="-lkernel_web" (
	set "LIBS_KERN="
    ) else if "%~1"=="-nostartfiles" (
        set "LIBS_CRT="
    ) else if "%~1"=="-c" (
        set "LIBS_CRT="
    ) else if "%~1"=="-shared" (
        set "LIBS_CRT="
    ) else if not "%ARG:~0,1%"=="-" (
        set "TU=%ARG%"
    )
shift
goto checkargs

:runcmd

if "%TU%"=="" (
    set "LIBS_CRT="
    set "LIBS_C="
    set "LIBS_DEPS="
    set "LIBS_KERN="
)

rem -nostdlibc is for this script, clang does not know it
set ARGS=%ARGS:-nostdlibc=%

rem ensure clang knows where @PREFIX@-@LINKER@.exe is at
set PATH=%PATH%;%@PLATFORM_UC@_PAYLOAD_SDK%\win

clang ^
    --start-no-unused-arguments ^
    -target x86_64-@VENDOR@-@PLATFORM@ ^
    -fvisibility-nodllstorageclass=default ^
    -isysroot "%@PLATFORM_UC@_PAYLOAD_SDK%" ^
    -isystem "%@PLATFORM_UC@_PAYLOAD_SDK%\target\include" ^
    -L "%@PLATFORM_UC@_PAYLOAD_SDK%\target\lib" ^
    -L "%@PLATFORM_UC@_PAYLOAD_SDK%\target\user\homebrew\lib" ^
    -fno-stack-protector -fno-plt -femulated-tls ^
    %LIBS_C% ^
    --end-no-unused-arguments ^
    %ARGS% ^
    --start-no-unused-arguments ^
    --sysroot "%@PLATFORM_UC@_PAYLOAD_SDK%" ^
    %LIBS_CRT% %LIBS_KERN% %LIBS_DEPS% ^
    --end-no-unused-arguments
