#!/usr/bin/env bash
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

@PLATFORM_UC@_PAYLOAD_SDK="${BASH_SOURCE[0]}"
@PLATFORM_UC@_PAYLOAD_SDK="$(dirname "${@PLATFORM_UC@_PAYLOAD_SDK}")"
@PLATFORM_UC@_PAYLOAD_SDK="${@PLATFORM_UC@_PAYLOAD_SDK}/.."
@PLATFORM_UC@_PAYLOAD_SDK="$(realpath "${@PLATFORM_UC@_PAYLOAD_SDK}")"

export @PLATFORM_UC@_PAYLOAD_SDK=${@PLATFORM_UC@_PAYLOAD_SDK}
export @PLATFORM_UC@_SYSROOT=${@PLATFORM_UC@_PAYLOAD_SDK}/target
export @PLATFORM_UC@_HBROOT=/user/homebrew

export DESTDIR="${@PLATFORM_UC@_SYSROOT}"
export PREFIX="${@PLATFORM_UC@_HBROOT}"

export @PLATFORM_UC@_DEPLOY=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-deploy
export @PLATFORM_UC@_CROSS_FIX_ROOT=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-cross-fix-root

export AS=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-as
export CC=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-clang
export CXX=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-clang++
export OBJC=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-clang
export OBJCXX=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-clang++
export LD=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-@LINKER@

export AR=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-ar
export NM=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-nm
export OBJCOPY=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-objcopy
export RANLIB=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-ranlib
export STRIP=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-strip
export STRINGS=strings

export MAKE=make
export CMAKE=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-cmake
export MESON=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-meson
export PKG_CONFIG=${@PLATFORM_UC@_PAYLOAD_SDK}/bin/@PREFIX@-pkg-config

