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
# <http://www.gnu.org/licenses/>.

# Included at the top of the Makefile of every sample, after it has set ELF.
#
# Everything is built in build. A sample that was installed with an SDK builds
# for that console, otherwise the goals run once per console that you have an
# SDK for (PS4_PAYLOAD_SDK, PS5_PAYLOAD_SDK), "make ps4" or "make ps5" picks
# one. Only when more than one console is built at once, each gets its own
# build/<console>. Changing the console cleans build first. A sample that only
# exists on one console sets ONLY.

SPACE    := $() $()
HERE     := $(dir $(lastword $(MAKEFILE_LIST)))
CAN      := $(or $(ONLY),ps4 ps5)
OWNER    := $(if $(wildcard $(HERE)../toolchain/orbis.mk),ps4,$(if $(wildcard $(HERE)../toolchain/prospero.mk),ps5))
CLEANING := $(if $(filter clean,$(MAKECMDGOALS)),$(if $(filter-out clean ps4 ps5,$(MAKECMDGOALS)),,1))

ifndef PLATFORM
    ifdef OWNER
        DEFAULT_CONSOLES := $(OWNER)
    else ifdef CLEANING
        DEFAULT_CONSOLES := $(CAN)
    else
        DEFAULT_CONSOLES := $(and $(filter ps4,$(CAN)),$(PS4_PAYLOAD_SDK),ps4) $(and $(filter ps5,$(CAN)),$(PS5_PAYLOAD_SDK),ps5)
        ifeq ($(or $(strip $(DEFAULT_CONSOLES)),$(filter ps4 ps5,$(MAKECMDGOALS))),)
            $(error set $(subst $(SPACE), or ,$(strip $(if $(filter ps4,$(CAN)),PS4_PAYLOAD_SDK) $(if $(filter ps5,$(CAN)),PS5_PAYLOAD_SDK))))
        endif
        ifeq ($(filter ps4 ps5,$(MAKECMDGOALS)),)
            $(foreach c,$(filter-out $(DEFAULT_CONSOLES),$(CAN)),$(info skipping $(c), its SDK is not set))
        endif
    endif
    include $(firstword $(wildcard $(HERE)platforms.mk $(HERE)../platforms.mk))
else
    ifneq ($(ONLY),)
        ifneq ($(ONLY),$(PLATFORM))
            $(error this sample is only for the $(ONLY))
        endif
    endif
    ifeq ($(PLATFORM),ps4)
        PLATFORM_UC := PS4
        PREFIX      := orbis
    else ifeq ($(PLATFORM),ps5)
        PLATFORM_UC := PS5
        PREFIX      := prospero
    else
        $(error unknown console $(PLATFORM), use ps4 or ps5)
    endif

    BUILD := build$(if $(MULTI),/$(PLATFORM))
    ELF   := $(addprefix $(BUILD)/,$(ELF))
    CLEAN ?= $(BUILD)

    ifndef CLEANING
        ifeq ($(OWNER),$(PLATFORM))
            $(PLATFORM_UC)_PAYLOAD_SDK ?= $(abspath $(HERE)..)
        endif
        ifndef $(PLATFORM_UC)_PAYLOAD_SDK
            $(error $(PLATFORM_UC)_PAYLOAD_SDK is undefined)
        endif
        ifndef MULTI
            ifneq ($(wildcard $(BUILD)/.console),)
                ifneq ($(shell cat $(BUILD)/.console),$(PLATFORM))
                    $(shell rm -rf $(BUILD))
                endif
            endif
        endif
        include $($(PLATFORM_UC)_PAYLOAD_SDK)/toolchain/$(PREFIX).mk

        $(PLATFORM_UC)_HOST ?= $(PLATFORM)
        $(PLATFORM_UC)_PORT ?= 9021

        SDK    := $($(PLATFORM_UC)_PAYLOAD_SDK)
        HOST   := $($(PLATFORM_UC)_HOST)
        PORT   := $($(PLATFORM_UC)_PORT)
        DEPLOY := $($(PLATFORM_UC)_DEPLOY)
    endif

    .DEFAULT_GOAL := all
    .PHONY: all clean test debug

    ifndef CLEANING
        $(ELF): .EXTRA_PREREQS := $(SDK)/target/lib/crt1.o
    endif

$(BUILD):
	mkdir -p $@
	echo $(PLATFORM) > $@/.console

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(CLEAN)
	$(if $(MULTI),rmdir build 2>/dev/null || true)

    ifndef NO_TEST
test: $(ELF)
	$(DEPLOY) -h $(HOST) -p $(PORT) $^
    endif

    ifndef NO_DEBUG
debug: $(ELF)
	gdb-multiarch \
	-ex "set architecture i386:x86-64" \
	-ex "target extended-remote $(HOST):2159" \
	-ex "file $(ELF)" \
	-ex "remote put $(ELF) /data/$(notdir $(ELF))" \
	-ex "set remote exec-file /data/$(notdir $(ELF))" \
	-ex "start"
    endif
endif