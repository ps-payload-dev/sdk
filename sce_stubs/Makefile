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

ifndef PLATFORM
include ../platforms.mk
else

BUILD := build/$(PLATFORM)

include make.$(PLATFORM)
include ../Makefile.inc

.DEFAULT_GOAL := all

CFLAGS := -ffreestanding -fno-builtin -nostdlib -fPIC
CFLAGS += -target $(TRIPLE) -fno-plt -fno-stack-protector
CFLAGS += -Wall -Werror -fvisibility-nodllstorageclass=default

SPRXS := $(wildcard *.sprx)
STUBS := $(addprefix $(PLATFORM)/,$(SPRXS:.sprx=.c))

SOURCES := $(sort $(notdir $(wildcard shared/*.c $(PLATFORM)/*.c)))
LIBS    := $(addprefix $(BUILD)/,$(SOURCES:.c=.so))

NID_DB     := aerolib.csv
NID_DB_URL := https://raw.githubusercontent.com/zecoxao/sce_symbols/main/$(NID_DB)

.PHONY: all clean install stubs
.DELETE_ON_ERROR:

all: $(LIBS)

stubs: $(NID_DB) $(STUBS)

$(BUILD):
	mkdir -p $@

$(NID_DB):
	wget -O $@ $(NID_DB_URL)

$(PLATFORM)/%.c: %.sprx | $(NID_DB)
	$(PYTHON) genstub.py $< > $@

$(BUILD)/%.o: $(PLATFORM)/%.c | $(BUILD)
	cd $(PLATFORM) && $(CC) -c $(CFLAGS) -o ../$@ $*.c

$(BUILD)/%.o: shared/%.c | $(BUILD)
	cd shared && $(CC) -c $(CFLAGS) -o ../$@ $*.c

$(BUILD)/%.so: $(BUILD)/%.o
	cd $(BUILD) && $(LD) -m elf_x86_64 -shared -soname $*.sprx -o $*.so $*.o

clean:
	rm -rf $(BUILD) $(NID_DB)

install: $(LIBS)
	install -d "$(DESTDIR)/target/lib"
	install $(LIBS) "$(DESTDIR)/target/lib"
	for l in $(LINKS); do ln -sf $${l%%:*} "$(DESTDIR)/target/lib/$${l##*:}"; done
endif
