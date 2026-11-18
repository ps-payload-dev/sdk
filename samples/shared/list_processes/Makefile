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

ELF := list_processes.elf

include $(firstword $(wildcard ../sample.mk ../../sample.mk))
ifdef PLATFORM

CFLAGS := -Wall -Werror -g

all: $(ELF)

$(ELF): main.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

endif
