#   Copyright (C) 2026 John Törnblom and ItsJokerZz
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

CFLAGS := -target $(TRIPLE) -fPIC  -fno-plt -fno-stack-protector
CFLAGS += -nobuiltininc -isystem ../include/freebsd/$(FREEBSD)
CFLAGS += -fvisibility-nodllstorageclass=default
CFLAGS += -O1 -D_LIBUFS

ASFLAGS := $(CFLAGS)

BUILD := build/$(PLATFORM)

SRCS := $(wildcard *.c)
OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o))

ARCHIVE := $(BUILD)/libufs.a

.PHONY: all clean install

all: $(ARCHIVE)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ARCHIVE): $(OBJS)
	$(AR) -rsc $@ $^

clean:
	rm -rf $(BUILD)

install: $(ARCHIVE)
	install -d "$(DESTDIR)/target/lib"
	install $^ "$(DESTDIR)/target/lib"
endif
