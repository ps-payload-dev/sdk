#!/usr/bin/env bash
#   Copyright (C) 2023 John Törnblom
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

# usage: ci.sh [<ps4 | ps5 | all>]
# build the SDK and the samples for each console, both if none is given

SCRIPTDIR="${BASH_SOURCE[0]}"
SCRIPTDIR="$(cd "$(dirname "${SCRIPTDIR}")" && pwd)"

cd "$SCRIPTDIR" || exit 1

CONSOLES="$*"
if [[ -z "$CONSOLES" || "$CONSOLES" == all ]]; then
    CONSOLES="ps4 ps5"
fi

for CONSOLE in $CONSOLES; do
    case "$CONSOLE" in
        ps4) SDK_VAR=PS4_PAYLOAD_SDK ;;
        ps5) SDK_VAR=PS5_PAYLOAD_SDK ;;
        *) echo "error: unknown console $CONSOLE, use ps4 or ps5"; exit 1 ;;
    esac

    (
    SDK=$(mktemp -d)
    trap 'rm -rf -- "$SDK"' EXIT
    export DESTDIR=$SDK

    NO_SETENV=1 make $CONSOLE clean install || exit 1
    export "$SDK_VAR=$SDK"
    "$SCRIPTDIR/libcxx.sh" $CONSOLE || exit 1

    make -C "$SDK/samples" $CONSOLE clean all || exit 1
    ) || exit 1
done