#   Copyright (C) 2025 John Törnblom
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

ELF := install_app.elf

NO_DEBUG := 1

include $(firstword $(wildcard ../sample.mk ../../sample.mk))
ifdef PLATFORM

ELF2SELF := ./make_fself.py

TITLE_ID := FAKE02932

CFLAGS := -Wall -Werror -g -DTITLE_ID=\"$(TITLE_ID)\"
LDADD  := -lSceIpmi -lSceAppInstUtil -lSceUserService -lSceSystemService

# the PS4 describes the app in a param.sfo, made from the param.json of the PS5
ifeq ($(PLATFORM),ps4)
PS4_SDK_VERSION ?= 0x03500000
AUTHID       := 0x3800000000000011
PARAM        := param.sfo
USER_PARAM   := $(BUILD)/param.sfo
SYSTEM_PARAM := $(BUILD)/param.sfo
else
PS4_SDK_VERSION ?= 0x01000000
AUTHID       := 0x3800000000000022
PARAM        := param.json
USER_PARAM   := $(TITLE_ID)/sce_sys/param.json
SYSTEM_PARAM := $(TITLE_ID)/sce_sys/param.json.system
endif

# the payload puts these files on the console itself
ASSETS := -DEBOOT=\"$(BUILD)/eboot.bin\" -DICON0=\"$(TITLE_ID)/icon0.png\" \
          -DPARAM=\"$(PARAM)\" -DUSER_PARAM=\"$(USER_PARAM)\" \
          -DSYSTEM_PARAM=\"$(SYSTEM_PARAM)\"

AUTHINFO := 00 00 00 00 00 00 00 00 00 00 00 00 00 1C 00 40 00 FF 00 00 00 00 \
            00 80 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 80 \
            00 40 00 40 00 00 00 00 00 00 00 80 00 00 00 00 00 00 00 08 00 40 \
            FF FF 00 00 00 F0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
            00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
            00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
            00 00 00 00

.INTERMEDIATE: $(BUILD)/eboot.elf

all: $(ELF)

$(ELF): payload.c $(BUILD)/eboot.bin $(TITLE_ID)/icon0.png $(USER_PARAM) $(SYSTEM_PARAM) | $(BUILD)
	$(CC) $(CFLAGS) $(ASSETS) $(LDADD) -o $@ $<

$(BUILD)/eboot.elf: $(BUILD)/eboot.o
	$(LD) --static -T eboot.x -o $@ $^

$(BUILD)/eboot.bin: $(BUILD)/eboot.elf
	$(ELF2SELF) --ptype fake --paid $(AUTHID) \
		    --auth-info "$(AUTHINFO)" \
		    --app-version $(PS4_SDK_VERSION) \
		    --fw-version $(PS4_SDK_VERSION) $^ $@

$(BUILD)/param.sfo: $(TITLE_ID)/sce_sys/param.json | $(BUILD)
	./json2sfo.py $< $@

$(TITLE_ID)/icon0.png:
	convert -size 512x512 -gravity center label:'$(TITLE_ID)' $@

endif
