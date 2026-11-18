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

.PHONY: all clean install

all:

clean:

install:
	install -d "$(DESTDIR)/target/include/$(PLATFORM)"
	tar -C freebsd/$(FREEBSD) -cf - . | tar -C "$(DESTDIR)/target/include" -xof -
	tar -C khronos -cf - . | tar -C "$(DESTDIR)/target/include" -xof -
	tar -C shared -cf - . | tar -C "$(DESTDIR)/target/include/$(PLATFORM)" -xof -
	tar -C $(PLATFORM) -cf - . | tar -C "$(DESTDIR)/target/include/$(PLATFORM)" -xof -

	install -d "$(DESTDIR)/target/user/homebrew/include"
	ln -sfn user/homebrew/include "$(DESTDIR)/target/include_common"
endif
