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

ELF := hello_so.elf
NO_DEBUG := 1

include $(firstword $(wildcard ../sample.mk ../../sample.mk))
ifdef PLATFORM

CFLAGS := -Wall -g -Wl,--as-needed

# DAG:
#  hello_so.elf
#      /data/libtest.so
#          libSceLibcInternal.sprx
#          /data/libA.so
#              libSceLibcInternal.sprx
#          /data/libB.so
#              libSceRandom.sprx


all: $(ELF) $(BUILD)/libtest.so

$(ELF): main.c $(BUILD)/libtest.so | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/libA.so: libA.c | $(BUILD)
	$(CC) $(CFLAGS) -shared -o $@ $^ -Wl,-soname=/data/$(@F)

$(BUILD)/libB.so: libB.c | $(BUILD)
	$(CC) $(CFLAGS) -shared -lSceRandom -o $@ $^ -Wl,-soname=/data/$(@F)

$(BUILD)/libtest.so: libtest.c $(BUILD)/libA.so $(BUILD)/libB.so | $(BUILD)
	$(CC) $(CFLAGS) -shared -o $@ $^ -Wl,-soname=/data/$(@F)

upload:
	curl -T $(BUILD)/libA.so ftp://$(HOST):2121/data/libA.so
	curl -T $(BUILD)/libB.so ftp://$(HOST):2121/data/libB.so
	curl -T $(BUILD)/libtest.so ftp://$(HOST):2121/data/libtest.so

debug: $(ELF)
	gdb \
	-ex "target extended-remote $(HOST):2159" \
	-ex "file $(ELF)" \
	-ex "remote put $(ELF) /data/$(notdir $(ELF))" \
	-ex "set remote exec-file /data/$(notdir $(ELF))" \
	-ex "start"
endif
