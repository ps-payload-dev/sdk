#   Copyright (C) 2024 John Törnblom
#   Copyright (C) 2026 ItsJokerZz
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING. If not see
# <http://www.gnu.org/licenses/>.

@PLATFORM_UC@_PAYLOAD_SDK := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
@PLATFORM_UC@_SYSROOT     := $(@PLATFORM_UC@_PAYLOAD_SDK)/target
@PLATFORM_UC@_HBROOT      := /user/homebrew

DESTDIR ?= $(@PLATFORM_UC@_SYSROOT)
PREFIX  ?= $(@PLATFORM_UC@_HBROOT)

@PLATFORM_UC@_DEPLOY := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-deploy

AS         := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-as
CC         := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-clang
CXX        := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-clang++
OBJC       := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-clang
OBJCXX     := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-clang++
LD         := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-@LINKER@
PKG_CONFIG := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-pkg-config
AR         := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-ar
NM         := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-nm
OBJCOPY    := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-objcopy
RANLIB     := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-ranlib
STRIP      := $(@PLATFORM_UC@_PAYLOAD_SDK)/bin/@PREFIX@-strip

STRINGS := strings
