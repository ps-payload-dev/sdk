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

include make.$(PLATFORM)
include ../Makefile.inc

.DEFAULT_GOAL := all

CFLAGS += -ffreestanding -fno-builtin -nostdlib -fPIC
CFLAGS += -target $(TRIPLE) -fno-plt -fno-stack-protector
CFLAGS += -fvisibility-nodllstorageclass=default -Wall -Werror
CFLAGS += -fms-extensions -Wno-microsoft-anon-tag
CFLAGS += -O1
CFLAGS += -Ishared -I$(PLATFORM) -I../include

# Rebuild objects when a header they include changes. Without this an edit to
# syscall.h or kernel.h leaves older objects in crt1.o.
CFLAGS += -MMD -MP

BUILD := build/$(PLATFORM)

vpath %.c shared $(PLATFORM)
vpath %.S shared $(PLATFORM)

RTLD_OBJ := rtld.o rtld_so.o rtld_sprx.o rtld_payload.o rtld_dlfcn.o

OBJ := crt.o syscall.o klog.o nid.o kernel.o kernel_shared.o $(RTLD_OBJ) \
       mdbg.o patch.o $(EXTRA_OBJ) _start.o
OBJS := $(addprefix $(BUILD)/,$(OBJ))

-include $(OBJS:.o=.d)

CRT := crt1.o crti.o crtn.o crtbegin.o crtend.o crtbeginS.o crtendS.o
CRTS := $(addprefix $(BUILD)/,$(CRT))

.PHONY: all clean install

all: $(CRTS)

$(BUILD):
	mkdir -p $@

$(BUILD)/crtbeginS.o $(BUILD)/crtendS.o $(BUILD)/crtbegin.o \
$(BUILD)/crtend.o $(BUILD)/crti.o $(BUILD)/crtn.o: | $(BUILD)
	$(AR) -rsc $@

$(BUILD)/crt1.o: $(OBJS)
	$(LD) -m elf_x86_64 -r $(CRT1_LDFLAGS) -o $@ $^

$(BUILD)/kernel.o: $(PLATFORM)/kernel.c | $(BUILD)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/kernel_shared.o: shared/kernel.c | $(BUILD)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) -c $(CFLAGS) -masm=intel -o $@ $<

clean:
	rm -rf $(BUILD)

install: $(CRTS)
	install -d "$(DESTDIR)/target/lib"
	install $^ "$(DESTDIR)/target/lib"
endif
