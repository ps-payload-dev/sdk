/* Copyright (C) 2024 John Törnblom
   Copyright (C) 2025 John Törnblom
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

#include "klog.h"
#include "kernel.h"
#include "syscall.h"

static int (*snprintf)(char *, unsigned long, const char *, ...) = 0;
static char *(*strerror)(int) = 0;
static int (*vsnprintf)(char *, unsigned long, const char *,
                        __builtin_va_list) = 0;

static int *(*__error)(void) = 0;

#define KLOG_BUFSIZE 0x230

/**
 * Get the name of the process we are running in.
 **/
static char *klog_label(char *buf, unsigned long size) {
  int pid = ps_syscall(SYS_getpid);

  buf[0] = 0;
  ps_syscall(SYS_thr_get_name, pid, buf, size);

  if (buf[0] == 0) {
    snprintf(buf, size, "pid:%d", pid);
  }

  return buf;
}

int klog_printf(const char *fmt, ...) {
  __builtin_va_list args;
  char sargs[0x200] = {0};
  char buf[KLOG_BUFSIZE];
  char lbl[32];

  if (!snprintf || !vsnprintf) {
    return -1;
  }

  __builtin_va_start(args, fmt);
  vsnprintf(sargs, sizeof sargs, fmt, args);
  __builtin_va_end(args);

  snprintf(buf, sizeof buf, "<118>[%s] %s", klog_label(lbl, sizeof(lbl)),
           sargs);

  return (int)ps_syscall(SYS_mdbg_service, 7, buf, 0);
}

int klog_puts(const char *s) {
  char buf[KLOG_BUFSIZE];
  char lbl[32];

  if (!snprintf) {
    return -1;
  }

  snprintf(buf, sizeof buf, "<118>[%s] %s\n", klog_label(lbl, sizeof(lbl)), s);

  return (int)ps_syscall(SYS_mdbg_service, 7, buf, 0);
}

int klog_perror(const char *s) {
  char buf[KLOG_BUFSIZE];
  char lbl[32];

  if (!snprintf || !strerror || !__error) {
    return -1;
  }

  snprintf(buf, sizeof buf, "<118>[%s] %s: %s\n", klog_label(lbl, sizeof(lbl)),
           s, strerror(*__error()));

  return (int)ps_syscall(SYS_mdbg_service, 7, buf, 0);
}

int __klog_init(void) {
  int err;

  if ((err = KERNEL_DLSYM(0x2, "snprintf", &snprintf))) {
    return err;
  }
  if ((err = KERNEL_DLSYM(0x2, "strerror", &strerror))) {
    return err;
  }
  if ((err = KERNEL_DLSYM(0x2, "vsnprintf", &vsnprintf))) {
    return err;
  }
  if ((err = KERNEL_DLSYM(0x2001, "__error", &__error))) {
    if ((err = KERNEL_DLSYM(0x1, "__error", &__error))) {
      return err;
    }
  }

  return 0;
}
