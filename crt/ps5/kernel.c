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

#include "kernel.h"
#include "elf.h"
#include "kernel_iommu.h"
#include "nid.h"
#include "syscall.h"

/**
 * standard macros.
 **/
#define IPPROTO_IPV6 41
#define IPV6_PKTINFO 46

#define ESRCH 3
#define EBADF 9
#define EFAULT 14
#define EINVAL 22
#define ENOSYS 78

#define MAP_ANONYMOUS 0x1000
#define MAP_PRIVATE 0x02
#define MAP_FAILED ((void *)-1)

#define PROT_READ 1
#define PROT_WRITE 2

#define IN6_PKTINFOSZ 5

// Keep the resolver file-local without colliding with libc's __error symbol.
static int *(*sdk__error)(void) = 0;

/**
 *
 **/
typedef union kernel_pipebuf {
  unsigned int n[IN6_PKTINFOSZ];
  struct __attribute__((packed)) {
    unsigned int cnt;
    unsigned int in;
    unsigned int out;
    unsigned long reserved;
  } flags;

  struct __attribute__((packed)) {
    unsigned int size;
    unsigned long kaddr;
    unsigned long reserved;
  } pbuf;

  struct __attribute__((packed)) {
    unsigned long kaddr;
    unsigned int reserved[3];
  } vbuf;
} kernel_pipebuf_t;

/**
 * public constants.
 **/
unsigned long KERNEL_ADDRESS_DMAP_BASE = 0;        // derived by crt
unsigned long KERNEL_ADDRESS_IOMMU_SOFTC = 0;      // derived by crt
unsigned long KERNEL_ADDRESS_PRISON0 = 0;          // derived by crt
unsigned long KERNEL_ADDRESS_UTOKEN_FLAGS = 0;     // derived by crt
unsigned long KERNEL_ADDRESS_QA_FLAGS = 0;         // derived by crt
unsigned long KERNEL_ADDRESS_TARGETID = 0;         // derived by crt

const unsigned long KERNEL_OFFSET_PROC_P_UCRED = 0x40;
const unsigned long KERNEL_OFFSET_PROC_P_FD = 0x48;
const unsigned long KERNEL_OFFSET_PROC_P_PID = 0xBC;
const unsigned long KERNEL_OFFSET_PROC_P_VMSPACE = 0x200;
const unsigned long KERNEL_OFFSET_PROC_P_DYNLIB = 0x3e8;

const unsigned long KERNEL_OFFSET_THREAD_TD_TID = 0x9c;

const unsigned long KERNEL_OFFSET_UCRED_CR_UID = 0x04;
const unsigned long KERNEL_OFFSET_UCRED_CR_RUID = 0x08;
const unsigned long KERNEL_OFFSET_UCRED_CR_SVUID = 0x0C;
const unsigned long KERNEL_OFFSET_UCRED_CR_RGID = 0x14;
const unsigned long KERNEL_OFFSET_UCRED_CR_SVGID = 0x18;

const unsigned long KERNEL_OFFSET_UCRED_CR_PRISON = 0x30;

const unsigned long KERNEL_OFFSET_UCRED_CR_SCEAUTHID = 0x58;
const unsigned long KERNEL_OFFSET_UCRED_CR_SCECAPS = 0x60;
const unsigned long KERNEL_OFFSET_UCRED_CR_SCEATTRS = 0x80;

const unsigned long KERNEL_OFFSET_FILEDESC_FD_CDIR = 0x08;
const unsigned long KERNEL_OFFSET_FILEDESC_FD_RDIR = 0x10;
const unsigned long KERNEL_OFFSET_FILEDESC_FD_JDIR = 0x18;

/**
 * private global variables.
 **/
static unsigned long pipe_addr = 0;
static int rw_pipe[2] = {-1, -1};
static int rw_pair[2] = {-1, -1};

#define MASTER_SOCK rw_pair[0]
#define VICTIM_SOCK rw_pair[1]

static int kernel_is_heap_addr(unsigned long kaddr) {
  unsigned int hi;

  if (!kaddr || (kaddr & 7)) {
    return 0;
  }
  if ((kaddr >> 48) != 0xffff) {
    return 0;
  }

  hi = (unsigned int)((kaddr >> 32) & 0xffff);
  if (hi == 0 || hi == 0xffff) {
    return 0;
  }

  return 1;
}

static unsigned long kernel_find_dmap_base(void) {
  unsigned long vmspace;
  unsigned long pml4u;
  unsigned long proc;
  unsigned long cr3;

  if (!(proc = kernel_get_proc(0))) {
    return 0;
  }
  if (kernel_copyout(proc + KERNEL_OFFSET_PROC_P_VMSPACE, &vmspace,
                     sizeof(vmspace))) {
    return 0;
  }

  if (kernel_copyout(vmspace + KERNEL_OFFSET_VMSPACE_VM_PMAP + 0x20, &pml4u,
                     sizeof(pml4u))) {
    return 0;
  }
  if (kernel_copyout(vmspace + KERNEL_OFFSET_VMSPACE_VM_PMAP + 0x28, &cr3,
                     sizeof(cr3))) {
    return 0;
  }

  return pml4u - cr3;
}

static unsigned long kernel_find_iommu_softc(void) {
  unsigned long softc;
  unsigned long kaddr;
  unsigned long paddr;

  for (kaddr = KERNEL_ADDRESS_ALLPROC; kaddr >= KERNEL_ADDRESS_DATA_BASE + 8;
       kaddr -= 8) {
    if (kernel_copyout(kaddr, &softc, sizeof(softc))) {
      return 0;
    }
    if (!kernel_is_heap_addr(softc)) {
      continue;
    }
    if (kernel_copyout(softc + 0x48, &paddr, sizeof(paddr))) {
      continue;
    }
    if (paddr == 0xfdd80000UL) {
      return softc;
    }
  }

  return 0;
}

/**
 * Read the firmware version from libSceLibcInternal.sprx since some payloads
 * modify the version reported by the kernel.
 **/
unsigned int kernel_get_fw_version(void) {
  struct Sce_Proc_Param {
    unsigned long structsize;
    unsigned int magic;
    unsigned int ent_count;
    unsigned int sdk_ps4_ver;
    unsigned int sdk_ps5_ver;
  } *sce_proc_param = 0;
  unsigned int handle = 0x2;

  if (ps_syscall(SYS_dynlib_get_obj_member, handle, 8, &sce_proc_param)) {
    return 0;
  }
  if (!sce_proc_param) {
    return 0;
  }

  return sce_proc_param->sdk_ps5_ver;
}

int __kernel_init(payload_args_t *args) {
   unsigned int fw = kernel_get_fw_version();

  if ((rw_pair[0] = args->rwpair[0]) < 0) {
    return -EBADF;
  }
  if ((rw_pair[1] = args->rwpair[1]) < 0) {
    return -EBADF;
  }

  if ((rw_pipe[0] = args->rwpipe[0]) < 0) {
    return -EBADF;
  }
  if ((rw_pipe[1] = args->rwpipe[1]) < 0) {
    return -EBADF;
  }

  if (!(pipe_addr = args->kpipe_addr)) {
    return -EFAULT;
  }
  if (!(KERNEL_ADDRESS_DATA_BASE = args->kdata_base_addr)) {
    return -EFAULT;
  }

   if(init_offsets(fw) == -ENOSYS) {
    return -ENOSYS;
  }
  
  KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_SECURITY_FLAGS + 0x09;
  KERNEL_ADDRESS_QA_FLAGS = KERNEL_ADDRESS_SECURITY_FLAGS + 0x24;
  KERNEL_ADDRESS_UTOKEN_FLAGS = KERNEL_ADDRESS_SECURITY_FLAGS + 0x8C;
  KERNEL_ADDRESS_PRISON0 = kernel_get_ucred_prison(0);

  if (!(KERNEL_ADDRESS_DMAP_BASE = kernel_find_dmap_base())) {
    return -ENOSYS;
  }
  if (!(KERNEL_ADDRESS_IOMMU_SOFTC = kernel_find_iommu_softc())) {
    return -ENOSYS;
  }

  if (KERNEL_DLSYM(0x1, "__error", &sdk__error)) {
    if (KERNEL_DLSYM(0x2001, "__error", &sdk__error)) {
      return -1;
    }
  }

  return 0;
}

void kernel_set_errno(int val) {
  if (sdk__error) {
    *sdk__error() = val;
  }
}

/**
 * Write data to an address in kernel space.
 **/
static int kernel_write(unsigned long kaddr, void *data, unsigned long len) {
  kernel_pipebuf_t buf = {0};

  // sanity check for invalid kernel pointers
  if (!(kaddr & 0xffff000000000000)) {
    SET_ERRNO(EFAULT);
    return -1;
  }

  buf.vbuf.kaddr = kaddr;

  if (ps_syscall(SYS_setsockopt, MASTER_SOCK, IPPROTO_IPV6, IPV6_PKTINFO, &buf,
                 sizeof(buf))) {
    return -1;
  }

  if (ps_syscall(SYS_setsockopt, VICTIM_SOCK, IPPROTO_IPV6, IPV6_PKTINFO, data,
                 len)) {
    return -1;
  }

  return 0;
}

int kernel_copyin(const void *uaddr, unsigned long kaddr, unsigned long len) {
  kernel_pipebuf_t buf = {0};

  if (!kaddr || !uaddr || !len) {
    SET_ERRNO(EINVAL);
    return -1;
  }

  buf.flags.reserved = 0x40000000;

  if (kernel_write(pipe_addr, &buf, sizeof(buf))) {
    return -1;
  }

  buf.pbuf.size = 0x40000000;
  buf.pbuf.kaddr = kaddr;
  buf.pbuf.reserved = 0;

  if (kernel_write(pipe_addr + 12, &buf, sizeof(buf))) {
    return -1;
  }

  if (ps_syscall(SYS_write, rw_pipe[1], uaddr, len) < 0) {
    return -1;
  }

  return 0;
}

int kernel_copyout(unsigned long kaddr, void *uaddr, unsigned long len) {
  kernel_pipebuf_t buf = {0};

  if (!kaddr || !uaddr || !len) {
    SET_ERRNO(EINVAL);
    return -1;
  }

  buf.flags.cnt = 0x40000000;
  buf.flags.in = 0x40000000;

  if (kernel_write(pipe_addr, &buf, sizeof(buf))) {
    return -1;
  }

  buf.pbuf.size = 0x40000000;
  buf.pbuf.kaddr = kaddr;
  buf.pbuf.reserved = 0;

  if (kernel_write(pipe_addr + 12, &buf, sizeof(buf))) {
    return -1;
  }

  if (ps_syscall(SYS_read, rw_pipe[0], uaddr, len) < 0) {
    return -1;
  }

  return 0;
}

/**
 *
 **/
static unsigned long kernel_get_proc_inp6_outputopts(int pid, int fd) {
  unsigned long inp6_outputopts;
  unsigned long so_pcb;
  unsigned long file;

  if (!(file = kernel_get_proc_file(pid, fd))) {
    return 0;
  }

  if (kernel_copyout(file + 0x18, &so_pcb, sizeof(so_pcb))) {
    return 0;
  }

  if (kernel_copyout(so_pcb + 0x120, &inp6_outputopts,
                     sizeof(inp6_outputopts))) {
    return 0;
  }

  return inp6_outputopts;
}

/**
 *
 **/
static int kernel_inc_so_count(int pid, int fd) {
  unsigned long file;
  int so_count;

  if (!(file = kernel_get_proc_file(pid, fd))) {
    return -1;
  }

  if (kernel_copyout(file, &so_count, sizeof(so_count))) {
    return -1;
  }

  so_count++;
  if (kernel_copyin(&so_count, file, sizeof(so_count))) {
    return -1;
  }
  return 0;
}

int kernel_overlap_sockets(int pid, int master_sock, int victim_sock) {
  unsigned long master_inp6_outputopts;
  unsigned long victim_inp6_outputopts;
  unsigned long pktinfo;
  unsigned int tclass;

  if (kernel_inc_so_count(pid, master_sock)) {
    return -1;
  }

  if (!(master_inp6_outputopts =
            kernel_get_proc_inp6_outputopts(pid, master_sock))) {
    return -1;
  }

  if (kernel_inc_so_count(pid, victim_sock)) {
    return -1;
  }

  if (!(victim_inp6_outputopts =
            kernel_get_proc_inp6_outputopts(pid, victim_sock))) {
    return -1;
  }

  pktinfo = victim_inp6_outputopts + 0x10;
  if (kernel_copyin(&pktinfo, master_inp6_outputopts + 0x10, sizeof(pktinfo))) {

    return -1;
  }

  tclass = 0x13370000;
  if (kernel_copyin(&tclass, master_inp6_outputopts + 0xc0, sizeof(tclass))) {
    return -1;
  }

  return 0;
}

/**
 * init_offsets() needs the private ENOSYS macro defined above, so it is
 * pulled in here instead of being its own translation unit.
 **/
#include "kernel_offsets.c"

