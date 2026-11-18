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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#if defined(__PROSPERO__)
#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <ps5/mdbg.h>
#else
#include <ps4/kernel.h>
#include <ps4/klog.h>
#include <ps4/mdbg.h>
#endif

#define PUTS(s)                                                                \
  {                                                                            \
    puts(s);                                                                   \
    klog_puts(s);                                                              \
  }

#define PRINTF(s, ...)                                                         \
  {                                                                            \
    printf(s, __VA_ARGS__);                                                    \
    klog_printf(s, __VA_ARGS__);                                               \
  }

#define PERROR(s)                                                              \
  {                                                                            \
    printf("%s:%d:%s: %s\n", __FILE__, __LINE__, s, strerror(errno));          \
    klog_printf("%s:%d:%s: %s\n", __FILE__, __LINE__, s, strerror(errno));     \
  }

typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
#if defined(__PROSPERO__)
  uint32_t app_type;
  char title_id[10];
#else
  char title_id[14];
#endif
  char unknown2[0x3c];
} app_info_t;

int sceKernelGetAppInfo(pid_t pid, app_info_t *info);

// ptrace on PS4 needs a kernel patch first (not used here); PS4 sticks to
// mdbg below, this escalation path is PS5-only.
#if defined(__PROSPERO__)
static int sys_ptrace(int request, pid_t pid, caddr_t addr, int data) {
  uint8_t privcaps[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  pid_t mypid = getpid();
  uint8_t caps[16];
  uint64_t authid;
  int ret;

  if (!(authid = kernel_get_ucred_authid(mypid))) {
    return -1;
  }
  if (kernel_get_ucred_caps(mypid, caps)) {
    return -1;
  }

  if (kernel_set_ucred_authid(mypid, 0x4800000000010003l)) {
    return -1;
  }
  if (kernel_set_ucred_caps(mypid, privcaps)) {
    return -1;
  }

  ret = (int)ps_syscall(SYS_ptrace, request, pid, addr, data);

  if (kernel_set_ucred_authid(mypid, authid)) {
    return -1;
  }
  if (kernel_set_ucred_caps(mypid, caps)) {
    return -1;
  }

  return ret;
}

#define PT_DETACH_ADDR ((caddr_t)0)
#endif

static pid_t find_pid(const char *name) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  pid_t mypid = getpid();
  pid_t pid = -1;
  size_t buf_size;
  void *buf;

  if (sysctl(mib, 4, 0, &buf_size, 0, 0)) {
    PERROR("sysctl");
    return -1;
  }

  if (!(buf = malloc(buf_size))) {
    PERROR("malloc");
    return -1;
  }

  if (sysctl(mib, 4, buf, &buf_size, 0, 0)) {
    PERROR("sysctl");
    free(buf);
    return -1;
  }

  for (void *ptr = buf; ptr < (buf + buf_size);) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;

    ptr += ki->ki_structsize;
    if (!strcmp(name, ki->ki_comm) && ki->ki_pid != mypid) {
      pid = ki->ki_pid;
    }
  }

  free(buf);

  return pid;
}

static char *bin2hex(const uint8_t *bin, char *hex, size_t binsize) {
  hex[0] = '0';
  hex[1] = 'x';

  for (size_t i = 0; i < binsize; ++i) {
    sprintf(&hex[i * 2 + 2], "%02x", bin[i]);
  }

  hex[binsize * 2 + 2] = '\0';

  return hex;
}

static void print_info(pid_t pid) {
  app_info_t app_info = {0};
  uint8_t qaflags[16] = {0};
  uint8_t attrs[32] = {0};
  uint8_t caps[16] = {0};
  char buf[32 * 3];

  kernel_get_qaflags(qaflags);
  kernel_get_ucred_caps(pid, caps);
  kernel_get_ucred_attrs(pid, attrs);
  sceKernelGetAppInfo(pid, &app_info);

  PUTS("Console info");
  PUTS("------------");
  PRINTF("Firmware:       %08x\n", kernel_get_fw_version());
  PRINTF("TARGETID:       0x%02x\n", kernel_getchar(KERNEL_ADDRESS_TARGETID));
  PRINTF("QA_FLAGS:       %s\n", bin2hex(qaflags, buf, sizeof(qaflags)));
  PUTS("");

  PUTS("Privileges");
  PUTS("----------");
  PRINTF("SCE titleid:    %s\n", app_info.title_id);
  PRINTF("SCE authid:     0x%016lx\n", kernel_get_ucred_authid(pid));
  PRINTF("SCE attrs:      %s\n", bin2hex(attrs, buf, sizeof(attrs)));
  PRINTF("SCE caps:       %s\n", bin2hex(caps, buf, sizeof(caps)));
  PRINTF("uid:            %d\n", getuid());
  PRINTF("euid:           %d\n", geteuid());
  PRINTF("jail vnode:     0x%lx\n", kernel_get_proc_jaildir(pid));
  PUTS("");
}

static int test_mdbg(pid_t pid) {
  intptr_t val = -1;
  intptr_t addr;
  int prot;

  if (!(addr = kernel_dynlib_entry_addr(pid, 0))) {
    PERROR("mdbg kernel_dynlib_entry_addr");
    return -1;
  }

  if (mdbg_copyout(pid, addr, &val, sizeof(val))) {
    PERROR("mdbg_copyout");
    return -1;
  } else {
    PUTS("mdbg_copyout: success");
  }

  if ((prot = kernel_get_vmem_protection(pid, addr, sizeof(val))) < 0) {
    PUTS("mdbg: kernel_get_vmem_protection failed");
    return -1;
  }

  if (kernel_set_vmem_protection(pid, addr, sizeof(val),
                                 prot | PROT_READ | PROT_WRITE)) {
    PUTS("mdbg: kernel_set_vmem_protection failed");
    return -1;
  }

  if (mdbg_copyin(pid, &val, addr, sizeof(val))) {
    PERROR("mdbg_copyin");
    kernel_set_vmem_protection(pid, addr, sizeof(val), prot);
    return -1;
  } else {
    PUTS("mdbg_copyin: success");
  }

  if (kernel_set_vmem_protection(pid, addr, sizeof(val), prot)) {
    PUTS("mdbg: kernel_set_vmem_protection failed");
    return -1;
  }

  return 0;
}

#if defined(__PROSPERO__)
static int test_ptrace(pid_t pid) {
  struct ptrace_io_desc iod = {0};
  intptr_t val = -1;
  intptr_t addr;
  int err = 0;
  int prot;

  if (sys_ptrace(PT_ATTACH, pid, 0, 0)) {
    PERROR("ptrace attach");
    return -1;
  } else {
    PUTS("ptrace attach: success");
  }

  if (waitpid(pid, 0, 0) == -1) {
    perror("ptrace waitpid");
    sys_ptrace(PT_DETACH, pid, PT_DETACH_ADDR, 0);
    return -1;
  }

  if (!(addr = kernel_dynlib_entry_addr(pid, 0))) {
    perror("ptrace kernel_dynlib_entry_addr");
    sys_ptrace(PT_DETACH, pid, PT_DETACH_ADDR, 0);
    return -1;
  }

  prot = kernel_get_vmem_protection(pid, addr, sizeof(val));
  kernel_set_vmem_protection(pid, addr, sizeof(val),
                             prot | PROT_READ | PROT_WRITE);

  iod.piod_op = PIOD_READ_D;
  iod.piod_offs = (void *)addr;
  iod.piod_addr = &val;
  iod.piod_len = sizeof(val);
  if (sys_ptrace(PT_IO, pid, (caddr_t)&iod, 0)) {
    PERROR("ptrace io read");
    err = -1;
  } else {
    PUTS("ptrace io read: success");
  }

  iod.piod_op = PIOD_WRITE_D;
  iod.piod_offs = (void *)addr;
  iod.piod_addr = &val;
  iod.piod_len = sizeof(val);
  if (sys_ptrace(PT_IO, pid, (caddr_t)&iod, 0)) {
    PERROR("ptrace io write");
    err = -1;
  } else {
    PUTS("ptrace io write: success");
  }

  kernel_set_vmem_protection(pid, addr, sizeof(val), prot);

  if (sys_ptrace(PT_DETACH, pid, PT_DETACH_ADDR, 0)) {
    PERROR("ptrace detach");
    err = -1;
  } else {
    PUTS("ptrace detach: success");
  }

  return err;
}
#endif

#define ERROR_MDBG 1
#define ERROR_PTRACE 2

int main(void) {
  pid_t pid;
  int ret = 0;

  print_info(getpid());

  if ((pid = find_pid("SceShellCore")) < 0) {
    PUTS("find_pid failed");
    return -1;
  }

  PUTS("Test results");
  PUTS("------------");

  if (test_mdbg(pid)) {
    ret |= ERROR_MDBG;
  }

#if defined(__PROSPERO__)
  if (test_ptrace(pid)) {
    ret |= ERROR_PTRACE;
  }
#endif

  return ret;
}
