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
include template.mk

# bin/@PREFIX@-nid is a script, make must not build it from bin/@PREFIX@-nid.c
MAKEFLAGS += --no-builtin-rules

DESTDIR ?= /opt/ps-payload-sdk/$(PLATFORM)

TOOLCHAINS := toolchain/@PREFIX@.sh \
              toolchain/@PREFIX@.mk \
              toolchain/@PREFIX@.cmake \
              toolchain/@PREFIX@.ini

TOOLS := bin/@PREFIX@-cmake \
         bin/@PREFIX@-llvm \
         bin/@PREFIX@-meson \
         bin/@PREFIX@-llvm-config \
         bin/@PREFIX@-clang \
         bin/@PREFIX@-clang++ \
         bin/@PREFIX@-deploy \
         bin/@PREFIX@-@LINKER@ \
         bin/@PREFIX@-pkg-config \
         bin/@PREFIX@-nid \
         bin/@PREFIX@-fetchpkg \
         bin/@PREFIX@-cross-fix-root \
         bin/@PREFIX@-setenv

LDSCRIPTS := elf_x86_64.x

.PHONY: all clean install setenv unsetenv

all:

setenv unsetenv:
	@test -x "$(DESTDIR)/bin/$(PREFIX)-setenv" || { echo "$(DESTDIR)/bin/$(PREFIX)-setenv not found, install the $(PLATFORM) SDK first"; exit 1; }
	"$(DESTDIR)/bin/$(PREFIX)-setenv" $(if $(filter unsetenv,$@),--remove)

install: install-tools install-symlinks install-ldscripts install-toolchains

install-ldscripts: $(LDSCRIPTS)
	install -d "$(DESTDIR)"
	install -d "$(DESTDIR)/ldscripts"
	install $^ "$(DESTDIR)/ldscripts"
	install -d "$(DESTDIR)/target/lib"
	ln -sf ../../ldscripts/elf_x86_64.x "$(DESTDIR)/target/lib/main.script"
	ln -sf ../../ldscripts/elf_x86_64.x "$(DESTDIR)/target/lib/prx.script"

install-toolchains: $(TOOLCHAINS)
	install -d "$(DESTDIR)"
	install -d "$(DESTDIR)/toolchain"
	$(call install-templates,$^,$(DESTDIR)/toolchain)

install-tools: $(TOOLS)
	install -d "$(DESTDIR)"
	install -d "$(DESTDIR)/bin"
	$(call install-templates,$^,$(DESTDIR)/bin)

install-symlinks:
	install -d "$(DESTDIR)"
	install -d "$(DESTDIR)/bin"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/llvm-ar"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/llvm-nm"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/llvm-objcopy"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/llvm-ranlib"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/llvm-strip"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/clang"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/clang++"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/clang-cpp"
	ln -sf $(PREFIX)-llvm "$(DESTDIR)/bin/ld.lld"
	ln -sf llvm-ar "$(DESTDIR)/bin/$(PREFIX)-ar"
	ln -sf llvm-nm "$(DESTDIR)/bin/$(PREFIX)-nm"
	ln -sf llvm-objcopy "$(DESTDIR)/bin/$(PREFIX)-objcopy"
	ln -sf llvm-ranlib "$(DESTDIR)/bin/$(PREFIX)-ranlib"
	ln -sf llvm-strip "$(DESTDIR)/bin/$(PREFIX)-strip"
	ln -sf $(PREFIX)-clang "$(DESTDIR)/bin/$(PREFIX)-as"
endif
