/* Copyright (C) 2025 John Törnblom
   Copyright (C) 2026 ItsJokerZz

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

#include <errno.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/syscall.h>

#include <shared/kernel.h>

#if defined(__ORBIS__)
#include <ps4/authid.h>
#define PTRACE_AUTHID SCE_AUTHID_SYSCORE
#elif defined(__PROSPERO__)
#define PTRACE_AUTHID 0x4800000000010003l
#endif


int
ptrace(int request, pid_t pid, caddr_t addr, int data) {
  uint64_t authid = 0;
  pid_t my_pid = getpid();
  int ret;

  // PT_TRACE_ME needs no elevated authid, and raising it here would race the
  // kernel copyin/copyout primitive against whatever already has this same
  // process open for ptrace (e.g. elfldr, mid elfldr_spawn, on the PS5).
  if(request != PT_TRACE_ME) {
    authid = kernel_get_ucred_authid(-1);
    kernel_set_ucred_authid(my_pid, PTRACE_AUTHID);
  }

  ret = syscall(SYS_ptrace, request, pid, addr, data);

  if(request != PT_TRACE_ME) {
    kernel_set_ucred_authid(my_pid, authid);
  }

  if(ret < 0) {
    errno = -ret;
    return -1;
  }

  return ret;
}
