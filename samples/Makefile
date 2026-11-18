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

# Builds, cleans and installs the samples of a console. Installed with an SDK it
# goes for that console, otherwise for the ones that you have an SDK for, like
# the Makefile of each sample.

HERE  := $(dir $(lastword $(MAKEFILE_LIST)))
OWNER := $(if $(wildcard $(HERE)../toolchain/orbis.mk),ps4,$(if $(wildcard $(HERE)../toolchain/prospero.mk),ps5))

ifndef PLATFORM
    DEFAULT_CONSOLES := $(if $(PS4_PAYLOAD_SDK),ps4) $(if $(PS5_PAYLOAD_SDK),ps5)
    ifneq ($(filter clean,$(MAKECMDGOALS)),)
        DEFAULT_CONSOLES := ps4 ps5
    endif
    ifdef OWNER
        DEFAULT_CONSOLES := $(OWNER)
    endif
    ifeq ($(or $(strip $(DEFAULT_CONSOLES)),$(filter ps4 ps5,$(MAKECMDGOALS))),)
        $(error set PS4_PAYLOAD_SDK or PS5_PAYLOAD_SDK)
    endif
    include $(firstword $(wildcard $(HERE)platforms.mk $(HERE)../platforms.mk))
else
    DESTDIR ?= /opt/ps-payload-sdk/$(PLATFORM)
    EXCLUDE := --exclude=build --exclude='*.elf' --exclude='*.o' --exclude='*.so' --exclude=eboot.bin

    # in the source tree the samples are in shared and in the folder of the console, installed they are side by side
    DIRS := $(dir $(wildcard */Makefile shared/*/Makefile $(PLATFORM)/*/Makefile))

    .DEFAULT_GOAL := all
    .PHONY: all clean install $(DIRS)

all clean: $(DIRS)

$(DIRS):
	$(MAKE) -C $@ PLATFORM=$(PLATFORM) $(MAKECMDGOALS)

install:
	install -d "$(DESTDIR)/samples"
	tar -C shared $(EXCLUDE) -cf - . | tar -C "$(DESTDIR)/samples" -xof -
	tar -C $(PLATFORM) $(EXCLUDE) -cf - . | tar -C "$(DESTDIR)/samples" -xof -
	install -m 644 Makefile sample.mk "$(DESTDIR)/samples"
	install -m 644 $(firstword $(wildcard platforms.mk ../platforms.mk)) "$(DESTDIR)/samples"
endif