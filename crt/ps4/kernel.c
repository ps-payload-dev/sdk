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

#include "kernel.h"
#include "nid.h"
#include "syscall.h"

/**
 * Private constants.
 **/
#define X86_CR0_WP (1 << 16)
#define MSR_LSTAR 0xC0000082

#define EFAULT 14
#define EINVAL 22
#define ENOSYS 78

#define MAP_ANONYMOUS 0x1000
#define MAP_PRIVATE 0x02
#define MAP_FAILED ((void *)-1)

#define PROT_READ 1
#define PROT_WRITE 2

/**
 * Forward declaration of the data structure used by FreeBSD to capture the
 *state of kernel threads.
 **/
struct thread;

/**
 * Arguments passed to the kexec syscall.
 **/
typedef struct kexec_args {
  int (*fn)(struct thread *, struct kexec_args *);
  long arg1;
  long arg2;
  long arg3;
  long arg4;
  long arg5;
} kexec_args_t;

/**
 * Context used by kexec_find_pattern().
 **/
typedef struct {
  unsigned long kaddr;
  unsigned int kaddr_len;
  char pattern[0x100];
  unsigned long *res;
} kexec_find_pattern_ctx_t;

/**
 * Data structure for the ELF header.
 **/
typedef struct {
  unsigned char e_ident[16];
  unsigned short e_type;
  unsigned short e_machine;
  unsigned int e_version;
  unsigned long e_entry;
  unsigned long e_phoff;
  unsigned long e_shoff;
  unsigned int e_flags;
  unsigned short e_ehsize;
  unsigned short e_phentsize;
  unsigned short e_phnum;
  unsigned short e_shentsize;
  unsigned short e_shnum;
  unsigned short e_shstrndx;
} Elf64_Ehdr;

/**
 * Data structure for the ELF program header.
 **/
typedef struct {
  unsigned int p_type;
  unsigned int p_flags;
  unsigned long p_offset;
  unsigned long p_vaddr;
  unsigned long p_paddr;
  unsigned long p_filesz;
  unsigned long p_memsz;
  unsigned long p_align;
} Elf64_Phdr;

typedef struct {
  unsigned int st_name;
  unsigned char st_info;
  unsigned char st_other;
  unsigned short st_shndx;
  unsigned long st_value;
  unsigned long st_size;
} Elf64_Sym;

/**
 * Public constants.
 **/

unsigned long KERNEL_ADDRESS_ALLPROC = 0;
unsigned long KERNEL_ADDRESS_ROOTVNODE = 0;
unsigned long KERNEL_ADDRESS_PRISON0 = 0;
unsigned long KERNEL_ADDRESS_QA_FLAGS = 0;

unsigned long KERNEL_ADDRESS_DMAP_BASE = 0xFFFFFE0000000000;

const unsigned long KERNEL_OFFSET_PROC_P_UCRED = 0x40;
const unsigned long KERNEL_OFFSET_PROC_P_FD = 0x48;
const unsigned long KERNEL_OFFSET_PROC_P_PID = 0xB0;
const unsigned long KERNEL_OFFSET_PROC_P_VMSPACE = 0x168;
const unsigned long KERNEL_OFFSET_PROC_P_DYNLIB = 0x340;

const unsigned long KERNEL_OFFSET_THREAD_TD_TID = 0x88;

const unsigned long KERNEL_OFFSET_UCRED_CR_UID = 0x04;
const unsigned long KERNEL_OFFSET_UCRED_CR_RUID = 0x08;
const unsigned long KERNEL_OFFSET_UCRED_CR_SVUID = 0x0C;
const unsigned long KERNEL_OFFSET_UCRED_CR_RGID = 0x14;
const unsigned long KERNEL_OFFSET_UCRED_CR_SVGID = 0x18;

const unsigned long KERNEL_OFFSET_UCRED_CR_PRISON = 0x30;

const unsigned long KERNEL_OFFSET_UCRED_CR_SCEAUTHID = 0x58;
const unsigned long KERNEL_OFFSET_UCRED_CR_SCECAPS = 0x60;
const unsigned long KERNEL_OFFSET_UCRED_CR_SCEATTRS = 0x83;

const unsigned long KERNEL_OFFSET_FILEDESC_FD_CDIR = 0x10;
const unsigned long KERNEL_OFFSET_FILEDESC_FD_RDIR = 0x18;
const unsigned long KERNEL_OFFSET_FILEDESC_FD_JDIR = 0x20;

/**
 * Convert a hexchar to a nibble (half byte).
 **/
static inline __attribute__((always_inline)) short hex2nib(char hexchar) {
  if (hexchar >= '0' && hexchar <= '9') {
    return hexchar - '0';
  }

  if (hexchar >= 'A' && hexchar <= 'F') {
    return hexchar - 'A' + 10;
  }

  if (hexchar >= 'a' && hexchar <= 'f') {
    return hexchar - 'a' + 10;
  }

  return -1;
}

/**
 * Check if a byte value matches a hexstring.
 **/
static inline __attribute__((always_inline)) short
byte_match(unsigned char bval, const char *hexstr) {
  short nib;

  if (!hexstr[0] || hexstr[0] == '?') {
    return 1;
  }

  if ((nib = hex2nib(hexstr[0])) < 0) {
    return -1;
  }

  if (nib != (bval >> 4)) {
    return -1;
  }

  if (!hexstr[1] || hexstr[1] == '?') {
    return 1;
  }

  if ((nib = hex2nib(hexstr[1])) < 0) {
    return -1;
  }

  if (nib != (bval & 0xf)) {
    return -1;
  }

  return 1;
}

/**
 * Check if a memory region matches a hexstring.
 **/
static inline __attribute__((always_inline)) int
pattern_match(const unsigned char *addr, unsigned int len, const char *hexstr) {
  while (len && *hexstr) {
    if (byte_match(*addr, hexstr) < 0) {
      return -1;
    }

    hexstr++;
    addr++;
    len--;

    if (*hexstr) {
      hexstr++;
    }
  }

  return 1;
}

/**
 * Scan a memory region for a pattern encoded by the given hexstring.
 **/
static inline __attribute__((always_inline)) const unsigned char *
pattern_scan(const unsigned char *addr, unsigned int len, const char *hexstr) {
  for (unsigned int i = 0; i < len; i++) {
    if (pattern_match(addr + i, len - i, hexstr) < 0) {
      continue;
    }

    return addr + i;
  }

  return 0;
}

/**
 * Invoke the given function from kernel space.
 **/
int kexec(int (*fn)(struct thread *, kexec_args_t *), ...) {
  long a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
  __builtin_va_list ap;
  int err;

  __builtin_va_start(ap, fn);
  a1 = __builtin_va_arg(ap, long);
  a2 = __builtin_va_arg(ap, long);
  a3 = __builtin_va_arg(ap, long);
  a4 = __builtin_va_arg(ap, long);
  a5 = __builtin_va_arg(ap, long);
  __builtin_va_end(ap);

  if ((err = ps_syscall(SYS_kexec, (long)fn, a1, a2, a3, a4, a5)) < 0) {
    return err;
  }

  return 0;
}

/**
 * Get the address of the invoking kernel thread (must be invoked from kernel
 *space).
 **/
static int kexec_get_thread(struct thread *td, kexec_args_t *args) {
  unsigned long *addr = (unsigned long *)args->arg1;

  if (!addr) {
    return EINVAL;
  }

  *addr = (long)td;

  return 0;
}

/**
 * Copy data from user space into kernel space (must be invoked from kernel
 *space).
 **/
static int kexec_copyin(struct thread *td, kexec_args_t *args) {
  int (*copyin)(const void *, void *, unsigned long) = (void *)args->arg1;
  const void *uaddr = (void *)args->arg2;
  void *kaddr = (void *)args->arg3;
  unsigned long len = args->arg4;
  unsigned long cr0;
  int ret;

  // read cr0
  asm volatile(".att_syntax\n"
               "mov %%cr0, %0"
               : "=r"(cr0));

  // clear cr0 write protection bits
  asm volatile(".att_syntax\n"
               "mov %0, %%cr0"
               :
               : "r"(cr0 & ~X86_CR0_WP));

  ret = copyin(uaddr, kaddr, len);

  // restore cr0
  asm volatile(".att_syntax\n"
               "mov %0, %%cr0"
               :
               : "r"(cr0));

  return ret;
}

/**
 * Copy data from kernel space into user space (must be invoked from kernel
 *space).
 **/
static int kexec_copyout(struct thread *td, kexec_args_t *args) {
  int (*copyout)(const void *, void *, unsigned long) = (void *)args->arg1;
  const void *kaddr = (void *)args->arg2;
  void *uaddr = (void *)args->arg3;
  unsigned long len = args->arg4;

  return copyout(kaddr, uaddr, len);
}

/**
 * Find a pattern in kernel memory (must be invoked from kernel space).
 **/
static int kexec_find_pattern(struct thread *td, kexec_args_t *args) {
  int (*copyin)(const void *, void *, unsigned long) = (void *)args->arg1;
  int (*copyout)(const void *, void *, unsigned long) = (void *)args->arg2;
  void *uctx = (void *)args->arg3;
  kexec_find_pattern_ctx_t ctx;
  unsigned long res;
  int err;

  if ((err = copyin(uctx, &ctx, sizeof(ctx)))) {
    return err;
  }

  ctx.pattern[sizeof(ctx.pattern) - 1] = 0;
  res = (long)pattern_scan((void *)ctx.kaddr, ctx.kaddr_len, ctx.pattern);

  return copyout(&res, ctx.res, sizeof(res));
}

unsigned int kernel_get_fw_version(void) {
  int mib[2] = {1, 38};
  unsigned long size = sizeof(mib);
  unsigned int version = 0;

  if (ps_syscall(SYS_sysctl, mib, 2, &version, &size, 0, 0)) {
    return 0;
  }

  return version;
}

int __kernel_init(void) {
  unsigned int fw = kernel_get_fw_version();
  unsigned long addr = 0;
  unsigned long prev = 0;
  int pid;
  int err;
  
  if ((err = init_offsets(fw)) == -ENOSYS) {
    return err;
  }

  // get the kernel address pointing at the current thread
  if ((err = kexec(kexec_get_thread, &addr))) {
    return err;
  }

  // get the kernel address pointing at the current proc
  if ((err = kernel_copyout(addr + 8, &addr, sizeof(addr)))) {
    return err;
  }

  // find 'allproc'
  while (addr) {
    if (kernel_copyout(addr + KERNEL_OFFSET_PROC_P_PID, &pid, sizeof(pid))) {
      return 0;
    }

    if (pid == 0) {
      break;
    }

    if (kernel_copyout(addr + 8, &prev, sizeof(prev))) {
      return 0;
    }

    addr = prev;
  }

  // ensure we found 'allproc'
  if (!(KERNEL_ADDRESS_ALLPROC = addr)) {
    return -1;
  }

  // get the address pointing at the global kernel variable 'prison0'
  if (!(KERNEL_ADDRESS_PRISON0 = kernel_get_ucred_prison(1))) {
    return -1;
  }

  // get the address pointing at the global kernel variable 'rootvnode'
  if (!(KERNEL_ADDRESS_ROOTVNODE = kernel_get_proc_rootdir(1))) {
    return -1;
  }

  KERNEL_ADDRESS_QA_FLAGS = KERNEL_ADDRESS_TARGETID + 0x1b;

  return 0;
}

int kernel_copyin(const void *uaddr, unsigned long kaddr, unsigned long len) {
  if (!kaddr || !uaddr) {
    return -EINVAL;
  }

  return kexec(kexec_copyin, KERNEL_ADDRESS_COPYIN, uaddr, kaddr, len);
}

int kernel_copyout(unsigned long kaddr, void *uaddr, unsigned long len) {
  if (!kaddr || !uaddr) {
    return -EINVAL;
  }

  return kexec(kexec_copyout, KERNEL_ADDRESS_COPYOUT, kaddr, uaddr, len);
}

unsigned long kernel_find_pattern(unsigned long addr, unsigned int len,
                                  const char *pattern) {
  kexec_find_pattern_ctx_t ctx;
  unsigned long res = 0;
  int pattern_len = -1;

  if (!addr || !len || !pattern) {
    return 0;
  }

  ctx.kaddr = addr;
  ctx.kaddr_len = len;
  ctx.res = &res;

  for (int i = 0; i < sizeof(ctx.pattern); i++) {
    ctx.pattern[i] = pattern[i];
    if (!pattern[i]) {
      pattern_len = i;
      break;
    }
  }

  if (pattern_len < 0 || pattern_len >= sizeof(ctx.pattern) - 1) {
    return 0;
  }

  if (kexec(kexec_find_pattern, KERNEL_ADDRESS_COPYIN, KERNEL_ADDRESS_COPYOUT,
            &ctx)) {
    return 0;
  }

  return res;
}

// on PS4 cr_sceattrs is 8 bytes, not 32 like on PS5; the rest of the array
// is unused and never touched in kernel memory
/**
 * init_offsets() needs kexec(), pattern_scan(), and the private error
 * macros defined above, so it is pulled in here instead of being its own
 * translation unit.
 **/
#include "kernel_offsets.c"
