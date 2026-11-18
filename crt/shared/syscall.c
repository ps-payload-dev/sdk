/* Copyright (C) 2024 John Törnblom
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

#include "syscall.h"

#if defined(__PROSPERO__)
static __attribute__((used)) long ptr_syscall = 0;

__attribute__((naked)) long __crt_syscall(long sysno, ...) {
  asm(".intel_syntax noprefix\n"
      "  mov rax, rdi\n"
      "  mov rdi, rsi\n"
      "  mov rsi, rdx\n"
      "  mov rdx, rcx\n"
      "  mov r10, r8\n"
      "  mov r8,  r9\n"
      "  mov r9,  qword ptr [rsp + 8]\n"
      "  sub rsp, 8\n"
      "  call qword ptr [rip + ptr_syscall]\n"
      "  add rsp, 8\n"
      "  ret\n");
}

int __crt_syscall_init(payload_args_t *args) {
  int (*sceKernelDlsym)(int, const char *, void *) = 0;

  if (!args || !args->sys_dynlib_dlsym) {
    return -1;
  }

  if (args->sys_dynlib_dlsym(0x1, "sceKernelDlsym", &sceKernelDlsym)) {
    args->sys_dynlib_dlsym(0x2001, "sceKernelDlsym", &sceKernelDlsym);
  }
  if (sceKernelDlsym == args->sys_dynlib_dlsym) {
    if (args->sys_dynlib_dlsym(0x1, "getpid", &ptr_syscall)) {
      args->sys_dynlib_dlsym(0x2001, "getpid", &ptr_syscall);
    }
  } else {
    ptr_syscall = (long)args->sys_dynlib_dlsym;
  }

  if (!ptr_syscall) {
    return -1;
  }

  ptr_syscall += 0xa;

  return 0;
}
#endif

long ps_syscall(long n, ...) {
  __builtin_va_list ap;
  long a1, a2, a3, a4, a5, a6;
  long ret;

  __builtin_va_start(ap, n);
  a1 = __builtin_va_arg(ap, long);
  a2 = __builtin_va_arg(ap, long);
  a3 = __builtin_va_arg(ap, long);
  a4 = __builtin_va_arg(ap, long);
  a5 = __builtin_va_arg(ap, long);
  a6 = __builtin_va_arg(ap, long);
  __builtin_va_end(ap);

#if defined(__PROSPERO__)
  ret = __crt_syscall(n, a1, a2, a3, a4, a5, a6);
#else
  ret = __syscall(n, a1, a2, a3, a4, a5, a6);
#endif

  return ret;
}
