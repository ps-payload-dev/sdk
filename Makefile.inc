#   Copyright (C) 2026 John Törnblom, Aleksei Kulaev and ItsJokerZz
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

MAKE    ?= make
PYTHON  ?= python3

DESTDIR ?= /opt/ps-payload-sdk/$(PLATFORM)

LLVM_CONFIG_CANDIDATES := $(foreach V,30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15,llvm-config-$(V)) \
                          llvm-config /opt/homebrew/opt/llvm/bin/llvm-config \
                          /usr/local/opt/llvm/bin/llvm-config

LLVM_CONFIG ?= $(firstword $(shell for c in $(LLVM_CONFIG_CANDIDATES); do command -v $$c 2>/dev/null && break; done))
ifeq ($(strip $(LLVM_CONFIG)),)
    $(error Unable to find LLVM, install LLVM 15 or newer with clang and lld, or set LLVM_CONFIG to its llvm-config)
endif

LLVM_BINDIR := $(shell $(LLVM_CONFIG) --bindir)
ifeq ($(LLVM_BINDIR),)
    $(error $(LLVM_CONFIG) does not tell where LLVM is, set LLVM_CONFIG to a working llvm-config)
endif
CC := $(LLVM_BINDIR)/clang
AR := $(LLVM_BINDIR)/llvm-ar
LD := $(LLVM_BINDIR)/ld.lld

# When using macos with brew, lld-19 and above are not installed in the standard
# llvm bindir directory, compensate for that here.
ifeq ($(shell command -v $(LD) 2>/dev/null),)
    OS := $(shell uname -s)
    ifeq ($(OS),Darwin)
        BREW := $(shell command -v brew 2>/dev/null)
        ifneq ($(BREW),)
            LD := $(shell $(BREW) --prefix lld 2>/dev/null)/bin/ld.lld
        else
            $(error Unable to find brew)
        endif
    endif
endif

ifeq ($(shell test -x "$(CC)" && echo ok),)
    $(error clang is not in $(LLVM_BINDIR), install it or set LLVM_CONFIG to the llvm-config of another LLVM)
endif
ifeq ($(shell test -x "$(AR)" && echo ok),)
    $(error llvm-ar is not in $(LLVM_BINDIR), install it or set LLVM_CONFIG to the llvm-config of another LLVM)
endif
ifeq ($(shell test -x "$(LD)" && echo ok),)
    $(error ld.lld was not found, install lld or set LLVM_CONFIG to the llvm-config of another LLVM)
endif
