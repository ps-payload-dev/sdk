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

SUBST := sed -E \
         -e '/^\#@$(OTHER)$$/,/^\#@end$$/d' \
         -e '/^\#@(ps4|ps5|end)$$/d' \
         -e 's/@PREFIX_UC@/$(PREFIX_UC)/g' \
         -e 's/@PREFIX@/$(PREFIX)/g' \
         -e 's/@PLATFORM_UC@/$(PLATFORM_UC)/g' \
         -e 's/@PLATFORM@/$(PLATFORM)/g' \
         -e 's/@VENDOR@/$(VENDOR)/g' \
         -e 's/@LINKER@/$(LINKER)/g'

define install-templates
for f in $(1); do \
    n=$$(basename "$$f" | $(SUBST)); \
    $(SUBST) "$$f" > "$(2)/$$n" && chmod 755 "$(2)/$$n" || exit 1; \
done
endef