/* Copyright (C) 2026 John Törnblom, idlesauce, sleirsgoevy and ItsJokerZz

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include "kernel.h"
#include "mdbg.h"

/**
 * mdbg_copyin()/mdbg_copyout() already provide full cross-process memory
 * access on PS4, so unlike PS5 there is no page-walk/DMAP fallback here.
 **/
int kernel_proc_copyin(int pid, const void *buf, unsigned long addr,
                       unsigned long len) {
  return mdbg_copyin(pid, buf, addr, len);
}

int kernel_proc_copyout(int pid, unsigned long addr, void *buf,
                        unsigned long len) {
  return mdbg_copyout(pid, addr, buf, len);
}
