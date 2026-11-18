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

# Without PLATFORM a Makefile runs its goals once per console. "make" and
# "make all" build ps4 and ps5, "make ps4" or "make ps5" only that one, and a
# console can be combined with a goal, as in "make ps5 install". "all" can be
# the console too, "make all install" installs both.
#
# "make install" puts the SDK of each console in /opt/ps-payload-sdk/<console>. With
# DESTDIR=<path>, a single console goes exactly there and several consoles go in
# <path>/<console>, the path can be absolute, relative or start with ~.
#
# A Makefile can set ONLY to the one console it is for, and DEFAULT_CONSOLES to the
# consoles that a plain make goes for.

MAIN     := $(firstword $(MAKEFILE_LIST))
CONSOLES := $(filter ps4 ps5,$(MAKECMDGOALS))
GOALS    := $(filter-out ps4 ps5,$(MAKECMDGOALS))

# in front of other goals "all" only names both consoles, as in "make all install"
ifeq ($(word 1,$(GOALS)),all)
    ifneq ($(word 2,$(GOALS)),)
        GOALS := $(wordlist 2,$(words $(GOALS)),$(GOALS))
    endif
endif

KNOWN := $(or $(ONLY),ps4 ps5)
DEFAULT_CONSOLES ?= $(KNOWN)

ifeq ($(CONSOLES),)
    CONSOLES := $(strip $(DEFAULT_CONSOLES))
endif

ifeq ($(CONSOLES),)
    $(error no console selected)
endif

ifneq ($(filter-out $(KNOWN),$(CONSOLES)),)
    $(error $(filter-out $(KNOWN),$(CONSOLES)) is not available here, only $(KNOWN))
endif

# Where an SDK goes: /opt/ps-payload-sdk/<console>, or DESTDIR for a single console,
# or a folder per console inside DESTDIR for several. A sourced toolchain file
# leaves its sysroot in DESTDIR, and an SDK is never installed into another one.
ifeq ($(origin DESTDIR),environment)
    ifeq ($(DESTDIR),$(PS4_SYSROOT))
        DESTDIR :=
    endif
endif
ifeq ($(origin DESTDIR),environment)
    ifeq ($(DESTDIR),$(PS5_SYSROOT))
        DESTDIR :=
    endif
endif

# the parts are made in their own folders, so the path has to be absolute
DESTROOT := $(patsubst ~/%,$(HOME)/%,$(patsubst ~,$(HOME),$(DESTDIR)))
ifneq ($(DESTROOT),)
    ifeq ($(filter /% %:/%,$(firstword $(DESTROOT))),)
        DESTROOT := $(CURDIR)/$(DESTROOT)
    endif
endif

ifneq ($(word 2,$(DESTROOT)),)
    $(warning "$(DESTROOT)" has a space, it installs there but the SDK does not work from such a path)
endif

sq = '$(subst ','\'',$(1))'

ifeq ($(DESTROOT),)
    DESTPATH := /opt/ps-payload-sdk/$$c
else ifneq ($(word 2,$(CONSOLES)),)
    DESTPATH := $(call sq,$(DESTROOT))/$$c
else
    DESTPATH := $(call sq,$(DESTROOT))
endif

unexport DESTDIR

.DEFAULT_GOAL := all
.PHONY: all ps4 ps5 $(GOALS) run

all ps4 ps5 $(GOALS): run

run:
	@for c in $(CONSOLES); do $(MAKE) -f $(MAIN) PLATFORM=$$c $(if $(word 2,$(CONSOLES)),MULTI=1) DESTDIR=$(DESTPATH) $(or $(GOALS),all) || exit 1; done
