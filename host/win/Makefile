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
include ../../platforms.mk
else

include ../make.$(PLATFORM)
include ../template.mk

DESTDIR ?= /opt/ps-payload-sdk/$(PLATFORM)

CC := x86_64-w64-mingw32-gcc
NINJA := https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip
PLINK := https://the.earth.li/~sgtatham/putty/latest/w64/plink.exe

SCRIPTS := @PREFIX@-clang.cmd @PREFIX@-clang++.cmd @PREFIX@-cmake.cmd \
           @PREFIX@-deploy.cmd @PREFIX@-setenv.cmd

TOOLS := $(PREFIX)-$(LINKER).exe ninja.exe plink.exe

.DELETE_ON_ERROR:

all: $(TOOLS)

$(PREFIX)-$(LINKER).c: @PREFIX@-@LINKER@.c
	$(SUBST) $< > $@

$(PREFIX)-$(LINKER).exe: $(PREFIX)-$(LINKER).c
	$(CC) -o $@ $^

ninja.exe:
	wget -q -O - $(NINJA) | gunzip -c -d > $@

plink.exe:
	wget -q -O $@ $(PLINK)

clean:
	rm -f *.exe *.o $(PREFIX)-$(LINKER).c

install: $(TOOLS) $(SCRIPTS)
	install -d "$(DESTDIR)/win"
	install $(TOOLS) "$(DESTDIR)/win"
	$(call install-templates,$(SCRIPTS),$(DESTDIR)/win)
endif
