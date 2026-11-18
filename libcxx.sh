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

# usage: libcxx.sh [--force] [<ps4 | ps5 | all>]
# fetch, build, and install libcxx into the SDK of each console, both if none is given.
# An SDK that already has it is left alone, --force builds it again. The llvm sources
# are fetched on the first run only, ~/.cache/ps-payload-sdk keeps them.

LLVM_URL="https://github.com/llvm/llvm-project/releases/download"
LLVM_VER="23.1.4"

FORCE=""
ARGS=()
for ARG in "$@"; do
    if [[ "$ARG" == --force ]]; then FORCE=1; else ARGS+=("$ARG"); fi
done

CONSOLES="${ARGS[*]}"
if [[ -z "$CONSOLES" || "$CONSOLES" == all ]]; then
    CONSOLES="ps4 ps5"
fi

console() {
    case "$1" in
        ps4) SDK_VAR=PS4_PAYLOAD_SDK; SYSROOT_VAR=PS4_SYSROOT; ENV_SH=orbis.sh ;;
        ps5) SDK_VAR=PS5_PAYLOAD_SDK; SYSROOT_VAR=PS5_SYSROOT; ENV_SH=prospero.sh ;;
        *) echo "error: unknown console $1, use ps4 or ps5"; exit 1 ;;
    esac
}

for CONSOLE in $CONSOLES; do
    console $CONSOLE
    if [[ -z "${!SDK_VAR}" ]]; then
        echo "error: $SDK_VAR is not set"
        exit 1
    fi
done

# built by this script for this version, or at least some libc++ is in there
libcxx_installed() { # sysroot
    if [[ -f "$1/.libcxx-version" ]]; then
        [[ "$(cat "$1/.libcxx-version")" == "$LLVM_VER" ]]
    else
        [[ -n "$(find "$1" -name libc++.a 2> /dev/null | head -n 1)" ]]
    fi
}

TODO=""
for CONSOLE in $CONSOLES; do
    console $CONSOLE
    if [[ -z "$FORCE" ]] && libcxx_installed "${!SDK_VAR}/target"; then
        echo "libc++ is already in ${!SDK_VAR}, nothing to do for $CONSOLE (--force builds it again)"
    else
        TODO="$TODO $CONSOLE"
    fi
done
[[ -z "$TODO" ]] && exit 0

# the sources are downloaded and unpacked once and then kept, PAYLOAD_SDK_CACHE moves them
CACHE="${PAYLOAD_SDK_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/ps-payload-sdk}"
LLVM_SRC="llvm-project-$LLVM_VER.src"
LLVM_TAR="$CACHE/downloads/$LLVM_SRC.tar.xz"

TEMPDIR=$(mktemp -d)
STAGE=""
trap 'rm -rf -- "$TEMPDIR" ${STAGE:+"$STAGE"}' EXIT

# a complete archive, xz notices a download that was cut short
archive_ok() {
    [[ -s "$1" ]] && { ! command -v xz > /dev/null || xz -t "$1" 2> /dev/null; }
}

if [[ ! -d "$CACHE/src/$LLVM_SRC/runtimes" ]]; then
    if ! archive_ok "$LLVM_TAR"; then
        mkdir -p "$CACHE/downloads" || exit 1
        wget -O "$LLVM_TAR.part" \
             $LLVM_URL/llvmorg-$LLVM_VER/$LLVM_SRC.tar.xz || { rm -f "$LLVM_TAR.part"; exit 1; }
        if ! archive_ok "$LLVM_TAR.part"; then
            rm -f "$LLVM_TAR.part"
            echo "error: the download of $LLVM_SRC is not a complete archive"
            exit 1
        fi
        mv "$LLVM_TAR.part" "$LLVM_TAR" || exit 1
    fi
    mkdir -p "$CACHE/src" || exit 1
    STAGE=$(mktemp -d "$CACHE/src/.stage.XXXXXX") || exit 1
    tar xf "$LLVM_TAR" -C "$STAGE" || exit 1
    mv "$STAGE/$LLVM_SRC" "$CACHE/src/$LLVM_SRC" 2> /dev/null ||
        [[ -d "$CACHE/src/$LLVM_SRC/runtimes" ]] || exit 1
    # under sudo the cache stays the one of the user
    if [[ "$(id -u)" == 0 && -n "$SUDO_USER" && "$SUDO_USER" != root ]]; then
        chown -R "$SUDO_USER" "$CACHE" 2> /dev/null
    fi
fi

for CONSOLE in $TODO; do
    console $CONSOLE
    (
    source ${!SDK_VAR}/toolchain/$ENV_SH || exit 1

    # Do not set CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY here. The probes then never link,
    # so every check_library_exists() passes and libc++ ends up asking the linker for rt,
    # atomic and gcc_s, which the sysroot does not have ("unable to find library from
    # dependent library specifier: rt").
    ${CMAKE} -S "$CACHE/src/$LLVM_SRC/runtimes" \
	 -B $TEMPDIR/build-$CONSOLE \
	 -DCMAKE_INSTALL_PREFIX="${!SYSROOT_VAR}" \
	 -DCMAKE_BUILD_TYPE=Release \
	 -DCMAKE_CXX_FLAGS="-nostdlib++" \
	 -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
	 -DLIBCXX_ENABLE_STATIC=YES \
	 -DLIBCXX_ENABLE_RTTI=YES \
	 -DLIBCXX_ENABLE_EXCEPTIONS=YES \
	 -DLIBCXX_ENABLE_THREADS=YES \
	 -DLIBCXX_ENABLE_SHARED=NO \
	 -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=NO \
	 -DLIBCXX_CXX_ABI=libcxxabi \
	 -DLIBCXXABI_ENABLE_STATIC=YES \
	 -DLIBCXXABI_ENABLE_EXCEPTIONS=YES \
	 -DLIBCXXABI_ENABLE_THREADS=YES \
	 -DLIBCXXABI_USE_LLVM_UNWINDER=YES \
	 -DLIBCXXABI_ENABLE_SHARED=NO \
	 -DLIBCXXABI_USE_COMPILER_RT=NO \
	 -DLIBUNWIND_ENABLE_STATIC=YES \
	 -DLIBUNWIND_ENABLE_THREADS=YES \
	 -DLIBUNWIND_IS_BAREMETAL=YES \
	 -DLIBUNWIND_ENABLE_SHARED=NO \
	 -DLIBUNWIND_USE_COMPILER_RT=NO || exit 1
    ${MAKE} -C $TEMPDIR/build-$CONSOLE install DESTDIR="/" || exit 1
    { echo "$LLVM_VER" > "${!SYSROOT_VAR}/.libcxx-version"; } 2> /dev/null || true
    ) || exit 1
done