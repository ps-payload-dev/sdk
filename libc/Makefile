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

ifndef PLATFORM
include ../platforms.mk
else

include make.$(PLATFORM)
include ../Makefile.inc

.DEFAULT_GOAL := all

CFLAGS := -target $(TRIPLE) -fPIC  -fno-plt -fno-stack-protector
CFLAGS += -nobuiltininc -isystem ../../include/freebsd/$(FREEBSD)
CFLAGS += -isystem ../../include
CFLAGS += -fvisibility-nodllstorageclass=default
CFLAGS += -O1

ASFLAGS := $(CFLAGS)

BUILD := build/$(PLATFORM)

SRCS := $(sort $(notdir $(wildcard shared/*.c $(PLATFORM)/*.c)))
OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o) _setjmp.o)

.PHONY: all clean install

all: $(BUILD)/libc.a $(BUILD)/libdl.a $(BUILD)/libpthread.a

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: shared/%.c | $(BUILD)
	cd shared && $(CC) $(CFLAGS) -c -o ../$@ $*.c

$(BUILD)/%.o: $(PLATFORM)/%.c | $(BUILD)
	cd $(PLATFORM) && $(CC) $(CFLAGS) -I../shared -c -o ../$@ $*.c

$(BUILD)/%.o: shared/%.S | $(BUILD)
	cd shared && $(CC) $(ASFLAGS) -c -o ../$@ $*.S

$(BUILD)/libc.a: $(OBJS)
	$(AR) -rsc $@ $^

$(BUILD)/libdl.a: | $(BUILD)
	$(AR) -rsc $@

$(BUILD)/libpthread.a: | $(BUILD)
	$(AR) -rsc $@

clean:
	rm -rf $(BUILD)

install: $(BUILD)/libc.a $(BUILD)/libdl.a $(BUILD)/libpthread.a
	install -d "$(DESTDIR)/target/lib"
	install $(BUILD)/libc.a "$(DESTDIR)/target/lib"
	install $(BUILD)/libdl.a "$(DESTDIR)/target/lib"
	install $(BUILD)/libpthread.a "$(DESTDIR)/target/lib"
endif
