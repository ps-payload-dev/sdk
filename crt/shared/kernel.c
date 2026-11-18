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

#include "kernel.h"
#include "elf.h"
#include "nid.h"
#include "syscall.h"

#define EINVAL 22
#define EFAULT 14
#define ENOSYS 78
#define ESRCH 3

#define MAP_ANONYMOUS 0x1000
#define MAP_PRIVATE 0x02
#define MAP_FAILED ((void *)-1)

#define PROT_READ 1
#define PROT_WRITE 2

#if defined(__PROSPERO__)
/**
 * performance tuning: cache up to 100 return values from kernel_get_proc()
 **/
static int proc_cache_counter = -1;
#define PROC_CACHE_SIZE 100
static struct {
  int pid;
  unsigned long addr;
} proc_cache[PROC_CACHE_SIZE] = {0};
#endif

/**
 * we need memcmp() before we can resolve symbols.
 **/
static int memcmp(const unsigned char *dst, const unsigned char *src,
                  unsigned long len) {
  for (unsigned long i = 0; i < len; i++) {
    if (dst[i] != src[i]) {
      return src[i] - dst[i];
    }
  }
  return 0;
}

/**
 * we need strcmp() before we can resolve symbols.
 **/
static int strncmp(const char *s1, const char *s2, unsigned long n) {
  if (n == 0) {
    return 0;
  }

  do {
    if (*s1 != *s2++) {
      return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
    }
    if (*s1++ == '\0') {
      break;
    }
  } while (--n != 0);

  return 0;
}

/**
 * we need strlen() before we can resolve symbols.
 **/
static unsigned long strlen(const char *str) {
  const char *start = str;

  while (*str) {
    str++;
  }

  return str - start;
}

int kernel_setlong(unsigned long addr, unsigned long val) {
  return kernel_copyin(&val, addr, sizeof(val));
}

int kernel_setint(unsigned long addr, unsigned int val) {
  return kernel_copyin(&val, addr, sizeof(val));
}

int kernel_setshort(unsigned long addr, unsigned short val) {
  return kernel_copyin(&val, addr, sizeof(val));
}

int kernel_setchar(unsigned long addr, unsigned char val) {
  return kernel_copyin(&val, addr, sizeof(val));
}

unsigned long kernel_getlong(unsigned long addr) {
  unsigned long val = 0;

  kernel_copyout(addr, &val, sizeof(val));

  return val;
}

unsigned int kernel_getint(unsigned long addr) {
  unsigned int val = 0;

  kernel_copyout(addr, &val, sizeof(val));

  return val;
}

unsigned short kernel_getshort(unsigned long addr) {
  unsigned short val = 0;

  kernel_copyout(addr, &val, sizeof(val));

  return val;
}

unsigned char kernel_getchar(unsigned long addr) {
  unsigned char val = 0;

  kernel_copyout(addr, &val, sizeof(val));

  return val;
}

int kernel_patch(unsigned long addr, const void *before, const void *after,
                 unsigned short size) {
  unsigned char current[size];

  // obtain current pattern
  if ((kernel_copyout(addr, current, size))) {
    return -1;
  }

  // already patched
  if (!memcmp(current, after, size)) {
    return 0;
  }

  // before does not match current, wrong offset?
  if (before && memcmp(before, current, size)) {
    return -1;
  }

  // apply patch
  if ((kernel_copyin(after, addr, size))) {
    return -1;
  }

  return 0;
}

unsigned long kernel_get_root_vnode(void) {
  unsigned long vnode = 0;

  if (kernel_copyout(KERNEL_ADDRESS_ROOTVNODE, &vnode, sizeof(vnode))) {
    return 0;
  }

  return vnode;
}

int kernel_dynlib_obj(int pid, unsigned int handle, dynlib_obj_t *obj) {
  unsigned long kproc;
  unsigned long kaddr;

  if (!(kproc = kernel_get_proc(pid))) {
    return -1;
  }

  if (kernel_copyout(kproc + KERNEL_OFFSET_PROC_P_DYNLIB, &kaddr,
                     sizeof(kaddr)) < 0) {
    return -1;
  }

  do {
    if (kernel_copyout(kaddr, &kaddr, sizeof(kaddr)) < 0) {
      return -1;
    }
    if (!kaddr) {
      SET_ERRNO(EINVAL);
      return -1;
    }

    if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, handle),
                       &obj->handle, sizeof(obj->handle)) < 0) {
      return -1;
    }
  } while (obj->handle != handle);

  if (kernel_copyout(kaddr, obj, sizeof(dynlib_obj_t)) < 0) {
    return -1;
  }

  return 0;
}

int kernel_dynlib_find_handle(int pid, unsigned long addr,
                              unsigned int *handle) {
  unsigned long mapbase;
  unsigned long mapsize;
  unsigned long kproc;
  unsigned long kaddr;
  unsigned long h;

  if (!(kproc = kernel_get_proc(pid))) {
    return -1;
  }

  if (kernel_copyout(kproc + KERNEL_OFFSET_PROC_P_DYNLIB, &kaddr,
                     sizeof(kaddr)) < 0) {
    return -1;
  }

  while (1) {
    if (kernel_copyout(kaddr, &kaddr, sizeof(kaddr)) < 0) {
      return -1;
    }
    if (!kaddr) {
      SET_ERRNO(EINVAL);
      return -1;
    }

    if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, mapbase),
                       &mapbase, sizeof(mapbase)) < 0) {
      return -1;
    }

    if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, mapsize),
                       &mapsize, sizeof(mapsize)) < 0) {
      return -1;
    }

    if (mapbase <= addr && addr <= mapbase + mapsize) {
      if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, handle), &h,
                         sizeof(h)) < 0) {
        return -1;
      }
      *handle = (unsigned int)h;
      return 0;
    }
  }

  return -1;
}

int kernel_dynlib_handle(int pid, const char *basename, unsigned int *handle) {
  unsigned long kproc;
  unsigned long kaddr;
  unsigned long kpath;
  unsigned long blen;
  unsigned long plen;
  char path[1024];
  long temphandle;

  if (!(kproc = kernel_get_proc(pid))) {
    return -1;
  }

  if (kernel_copyout(kproc + KERNEL_OFFSET_PROC_P_DYNLIB, &kaddr,
                     sizeof(kaddr)) < 0) {
    return -1;
  }

  blen = strlen(basename);
  do {
    if (kernel_copyout(kaddr, &kaddr, sizeof(kaddr)) < 0) {
      return -1;
    }
    if (!kaddr) {
      SET_ERRNO(EINVAL);
      return -1;
    }
    if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, path), &kpath,
                       sizeof(kpath)) < 0) {
      return -1;
    }
    if (kernel_copyout(kpath, path, sizeof(path)) < 0) {
      return -1;
    }

    if (kernel_copyout(kaddr + __builtin_offsetof(dynlib_obj_t, handle),
                       &temphandle, sizeof(temphandle)) < 0) {
      return -1;
    }

    plen = strlen(path);
    if (plen <= blen) {
      continue;
    }
    if (path[plen - blen - 1] != '/') {
      continue;
    }
    if (strncmp(path + plen - blen, basename, blen)) {
      continue;
    }

    *handle = (unsigned int)temphandle;
    return 0;

  } while (1);
}

int kernel_dynlib_path(int pid, unsigned int handle, char *path,
                       unsigned long size) {
  dynlib_obj_t obj;

  if (size > 1024) {
    size = 1024;
  }
  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return -1;
  }
  if (kernel_copyout(obj.path, path, size) < 0) {
    return -1;
  }

  path[size - 1] = 0;

  return 0;
}

unsigned long kernel_dynlib_mapbase_addr(int pid, unsigned int handle) {
  dynlib_obj_t obj;

  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return 0;
  }

  return obj.mapbase;
}

unsigned long kernel_dynlib_entry_addr(int pid, unsigned int handle) {
  dynlib_obj_t obj;

  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return 0;
  }

  return obj.entry;
}

unsigned long kernel_dynlib_init_addr(int pid, unsigned int handle) {
  dynlib_obj_t obj;

  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return 0;
  }

  return obj.init;
}

unsigned long kernel_dynlib_fini_addr(int pid, unsigned int handle) {
  dynlib_obj_t obj;

  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return 0;
  }

  return obj.fini;
}

unsigned long kernel_dynlib_resolve(int pid, int handle, const char *nid) {
  unsigned long vaddr = 0;
  dynlib_dynsec_t dynsec;
  dynlib_obj_t obj;
  Elf64_Sym *sym;
  char *buf_start;
  long buf_size;
  char *strtab;
  char *symtab;

  if (kernel_dynlib_obj(pid, handle, &obj)) {
    return 0;
  }
  if (kernel_copyout(obj.dynsec, &dynsec, sizeof(dynsec)) < 0) {
    return 0;
  }

  buf_size = dynsec.symtabsize + dynsec.strtabsize;
  if ((buf_start = (char *)ps_syscall(
           SYS_mmap, 0l, buf_size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0l)) == MAP_FAILED) {
    return 0;
  }

  symtab = buf_start;
  strtab = buf_start + dynsec.symtabsize;

  if (kernel_copyout(dynsec.symtab, symtab, dynsec.symtabsize) < 0) {
    ps_syscall(SYS_munmap, buf_start, buf_size);
    return 0;
  }
  if (kernel_copyout(dynsec.strtab, strtab, dynsec.strtabsize) < 0) {
    ps_syscall(SYS_munmap, buf_start, buf_size);
    return 0;
  }

  for (unsigned long i = 0; i < dynsec.symtabsize / sizeof(Elf64_Sym); i++) {
    sym = (Elf64_Sym *)(symtab + sizeof(Elf64_Sym) * i);
    if (!sym->st_value) {
      continue;
    }
    if (!strncmp(nid, strtab + sym->st_name, 11)) {
      vaddr = obj.mapbase + sym->st_value;
      break;
    }
  }

  ps_syscall(SYS_munmap, buf_start, buf_size);

  return vaddr;
}

unsigned long kernel_dynlib_dlsym(int pid, unsigned int handle,
                                  const char *sym) {
  char nid[12];

  nid_encode(sym, nid);

  return kernel_dynlib_resolve(pid, handle, nid);
}

unsigned long kernel_get_proc(int pid) {
  unsigned int other_pid = 0;
  unsigned long addr = 0;
  unsigned long next = 0;

  if (pid <= 0) {
    pid = ps_syscall(SYS_getpid);
  }

#if defined(__PROSPERO__)
  for (int i = 0; i < PROC_CACHE_SIZE; i++) {
    if (proc_cache[i].pid == pid) {
      return proc_cache[i].addr;
    }
  }
#endif

  if (kernel_copyout(KERNEL_ADDRESS_ALLPROC, &addr, sizeof(addr))) {
    return 0;
  }

  while (addr) {
    if (kernel_copyout(addr + KERNEL_OFFSET_PROC_P_PID, &other_pid,
                       sizeof(other_pid))) {
      return 0;
    }

    if (pid == other_pid) {
      break;
    }

    if (kernel_copyout(addr, &next, sizeof(next))) {
      return 0;
    }

    addr = next;
  }

#if defined(__PROSPERO__)
  if (!addr) {
    SET_ERRNO(ESRCH);
    return 0;
  }

  proc_cache_counter++;
  if (proc_cache_counter >= PROC_CACHE_SIZE) {
    proc_cache_counter = 0;
  }

  proc_cache[proc_cache_counter].pid = pid;
  proc_cache[proc_cache_counter].addr = addr;
#endif

  return addr;
}

unsigned long kernel_get_proc_thread(int pid, int tid) {
  unsigned long proc = kernel_get_proc(pid);

  if (!proc) {
    return 0;
  }

  if (tid < 0) {
    ps_syscall(SYS_thr_self, &tid);
  }

  for (long thr = kernel_getlong(proc + 0x10); thr;
       thr = kernel_getlong(thr + 0x10)) {
    if ((int)kernel_getlong(thr + KERNEL_OFFSET_THREAD_TD_TID) == (int)tid) {
      return thr;
    }
  }

  SET_ERRNO(EINVAL);

  return 0;
}

unsigned long kernel_get_proc_filedesc(int pid) {
  unsigned long filedesc = 0;
  unsigned long proc = 0;

  if (!(proc = kernel_get_proc(pid))) {
    return 0;
  }

  if (kernel_copyout(proc + KERNEL_OFFSET_PROC_P_FD, &filedesc,
                     sizeof(filedesc))) {
    return 0;
  }

  return filedesc;
}

unsigned long kernel_get_proc_file(int pid, int fd) {
  unsigned long table_base;
  unsigned long file;
  unsigned long proc;
  unsigned long p_fd;
#if defined(__ORBIS__)
  unsigned long entry_off = sizeof(void *) * fd;
#else
  unsigned long fde_file;
  unsigned long entry_off = 8 + (0x30 * fd);
#endif

  if (!(proc = kernel_get_proc(pid))) {
    return 0;
  }

  if (kernel_copyout(proc + KERNEL_OFFSET_PROC_P_FD, &p_fd, sizeof(p_fd))) {
    return 0;
  }

  if (kernel_copyout(p_fd, &table_base, sizeof(table_base))) {
    return 0;
  }

#if defined(__ORBIS__)
  if (kernel_copyout(table_base + entry_off, &file, sizeof(file))) {
    return 0;
  }
#else
  if (kernel_copyout(table_base + entry_off, &fde_file, sizeof(fde_file))) {
    return 0;
  }
  if (kernel_copyout(fde_file, &file, sizeof(file))) {
    return 0;
  }
#endif

  return file;
}

unsigned long kernel_get_proc_ucred(int pid) {
  unsigned long proc = 0;
  unsigned long ucred = 0;

  if (!(proc = kernel_get_proc(pid))) {
    return 0;
  }

  if (kernel_copyout(proc + KERNEL_OFFSET_PROC_P_UCRED, &ucred,
                     sizeof(ucred))) {
    return 0;
  }

  return ucred;
}

int kernel_get_ucred_attrs(int pid, unsigned char attrs[32]) {
  unsigned long ucred = 0;
#if defined(__ORBIS__)
  unsigned long len = 8;

  for (int i = 0; i < 32; i++) {
    attrs[i] = 0;
  }
#else
  unsigned long len = 32;
#endif

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_SCEATTRS, attrs, len)) {
    return -1;
  }

  return 0;
}

int kernel_set_ucred_attrs(int pid, const unsigned char attrs[32]) {
  unsigned long ucred = 0;
#if defined(__ORBIS__)
  unsigned long len = 8;
#else
  unsigned long len = 32;
#endif

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(attrs, ucred + KERNEL_OFFSET_UCRED_CR_SCEATTRS, len)) {
    return -1;
  }

  return 0;
}

unsigned long kernel_get_ucred_prison(int pid) {
  unsigned long prison = 0;
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return 0;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_PRISON, &prison,
                     sizeof(prison))) {
    return 0;
  }

  return prison;
}

int kernel_set_ucred_prison(int pid, unsigned long prison) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&prison, ucred + KERNEL_OFFSET_UCRED_CR_PRISON,
                    sizeof(prison))) {
    return -1;
  }

  return 0;
}

unsigned long kernel_get_ucred_authid(int pid) {
  unsigned long authid = 0;
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return 0;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_SCEAUTHID, &authid,
                     sizeof(authid))) {
    return 0;
  }

  return authid;
}

int kernel_set_ucred_authid(int pid, unsigned long authid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&authid, ucred + KERNEL_OFFSET_UCRED_CR_SCEAUTHID,
                    sizeof(authid))) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_caps(int pid, unsigned char caps[16]) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_SCECAPS, caps, 16)) {
    return -1;
  }

  return 0;
}

int kernel_set_ucred_caps(int pid, const unsigned char caps[16]) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(caps, ucred + KERNEL_OFFSET_UCRED_CR_SCECAPS, 16)) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_uid(int pid) {
  unsigned long ucred = 0;
  int uid = -1;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_UID, &uid, sizeof(uid))) {
    return -1;
  }

  return uid;
}

int kernel_set_ucred_uid(int pid, unsigned int uid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&uid, ucred + KERNEL_OFFSET_UCRED_CR_UID, sizeof(uid))) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_ruid(int pid) {
  unsigned long ucred = 0;
  int ruid = -1;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_RUID, &ruid,
                     sizeof(ruid))) {
    return -1;
  }

  return ruid;
}

int kernel_set_ucred_ruid(int pid, unsigned int ruid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&ruid, ucred + KERNEL_OFFSET_UCRED_CR_RUID, sizeof(ruid))) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_svuid(int pid) {
  unsigned long ucred = 0;
  int svuid = -1;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_SVUID, &svuid,
                     sizeof(svuid))) {
    return -1;
  }

  return svuid;
}

int kernel_set_ucred_svuid(int pid, unsigned int svuid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&svuid, ucred + KERNEL_OFFSET_UCRED_CR_SVUID,
                    sizeof(svuid))) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_rgid(int pid) {
  unsigned long ucred = 0;
  int rgid = -1;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_RGID, &rgid,
                     sizeof(rgid))) {
    return -1;
  }

  return rgid;
}

int kernel_set_ucred_rgid(int pid, unsigned int rgid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&rgid, ucred + KERNEL_OFFSET_UCRED_CR_RGID, sizeof(rgid))) {
    return -1;
  }

  return 0;
}

int kernel_get_ucred_svgid(int pid) {
  unsigned long ucred = 0;
  int svgid = -1;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyout(ucred + KERNEL_OFFSET_UCRED_CR_SVGID, &svgid,
                     sizeof(svgid))) {
    return -1;
  }

  return svgid;
}

int kernel_set_ucred_svgid(int pid, unsigned int svgid) {
  unsigned long ucred = 0;

  if (!(ucred = kernel_get_proc_ucred(pid))) {
    return -1;
  }

  if (kernel_copyin(&svgid, ucred + KERNEL_OFFSET_UCRED_CR_SVGID,
                    sizeof(svgid))) {
    return -1;
  }

  return 0;
}

unsigned long kernel_get_proc_rootdir(int pid) {
  unsigned long filedesc = 0;
  unsigned long vnode = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyout(filedesc + KERNEL_OFFSET_FILEDESC_FD_RDIR, &vnode,
                     sizeof(vnode))) {
    return 0;
  }

  return vnode;
}

unsigned long kernel_get_proc_jaildir(int pid) {
  unsigned long filedesc = 0;
  unsigned long vnode = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyout(filedesc + KERNEL_OFFSET_FILEDESC_FD_JDIR, &vnode,
                     sizeof(vnode))) {
    return 0;
  }

  return vnode;
}

int kernel_set_proc_rootdir(int pid, unsigned long vnode) {
  unsigned long filedesc = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyin(&vnode, filedesc + KERNEL_OFFSET_FILEDESC_FD_RDIR,
                    sizeof(vnode))) {
    return -1;
  }

  return 0;
}

int kernel_set_proc_jaildir(int pid, unsigned long vnode) {
  unsigned long filedesc = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyin(&vnode, filedesc + KERNEL_OFFSET_FILEDESC_FD_JDIR,
                    sizeof(vnode))) {
    return -1;
  }

  return 0;
}

unsigned long kernel_get_proc_currdir(int pid) {
  unsigned long filedesc = 0;
  unsigned long vnode = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyout(filedesc + KERNEL_OFFSET_FILEDESC_FD_CDIR, &vnode,
                     sizeof(vnode))) {
    return 0;
  }

  return vnode;
}

int kernel_set_proc_currdir(int pid, unsigned long vnode) {
  unsigned long filedesc = 0;

  if (!(filedesc = kernel_get_proc_filedesc(pid))) {
    return 0;
  }

  if (kernel_copyin(&vnode, filedesc + KERNEL_OFFSET_FILEDESC_FD_CDIR,
                    sizeof(vnode))) {
    return -1;
  }

  return 0;
}

int kernel_get_qaflags(unsigned char qaflags[16]) {
  return kernel_copyout(KERNEL_ADDRESS_QA_FLAGS, qaflags, 16);
}

int kernel_set_qaflags(const unsigned char qaflags[16]) {
#if defined(__PROSPERO__)
  if (kernel_get_fw_version() >= 0x7000000) {
    return kernel_iommu_copyin(qaflags, KERNEL_ADDRESS_QA_FLAGS, 16);
  }
#endif
  return kernel_copyin(qaflags, KERNEL_ADDRESS_QA_FLAGS, 16);
}

unsigned long kernel_get_vmem_entry(int pid, unsigned long addr) {
  unsigned long vm_map_entry_addr;
  unsigned long vmspace_addr;
  unsigned long proc_addr;
  unsigned long start;
  unsigned long end;

#if defined(__ORBIS__)
  if (!KERNEL_OFFSET_VMSPACE_P_ROOT) {
    return 0;
  }
#endif

  if (!(proc_addr = kernel_get_proc(pid))) {
    return 0;
  }

  if (kernel_copyout(proc_addr + KERNEL_OFFSET_PROC_P_VMSPACE, &vmspace_addr,
                     sizeof(vmspace_addr))) {
    return 0;
  }

  if (kernel_copyout(vmspace_addr + KERNEL_OFFSET_VMSPACE_P_ROOT,
                     &vm_map_entry_addr, sizeof(vm_map_entry_addr))) {
    return 0;
  }

  while (vm_map_entry_addr) {
    if (kernel_copyout(vm_map_entry_addr + 0x20, &start, sizeof(start))) {
      return 0;
    }
    if (kernel_copyout(vm_map_entry_addr + 0x28, &end, sizeof(end))) {
      return 0;
    }

    if (addr < start) {
      // left
      if (kernel_copyout(vm_map_entry_addr + 0x10, &vm_map_entry_addr,
                         sizeof(vm_map_entry_addr))) {
        return 0;
      }
    } else if (addr >= end) {
      // right
      if (kernel_copyout(vm_map_entry_addr + 0x18, &vm_map_entry_addr,
                         sizeof(vm_map_entry_addr))) {
        return 0;
      }
    } else {
      return vm_map_entry_addr;
    }
  }

#if defined(__ORBIS__)
  return EFAULT;
#else
  SET_ERRNO(EFAULT);
  return 0;
#endif
}

int kernel_get_vmem_protection(int pid, unsigned long addr, unsigned long len) {
  unsigned long vm_entry;
  unsigned char vm_prot;
  unsigned long start;
  int first = 1;
  int prot = -1;
#if defined(__ORBIS__)
  unsigned long vm_prot_off = 0x5c;
#else
  unsigned long vm_prot_off = 0x64;
#endif

#if defined(__ORBIS__)
  if (!KERNEL_OFFSET_VMSPACE_P_ROOT) {
    return ENOSYS;
  }
#endif

  if (!(vm_entry = kernel_get_vmem_entry(pid, addr))) {
    return -1;
  }

  while (vm_entry) {
    if (kernel_copyout(vm_entry + 0x20, &start, sizeof(start))) {
      return -1;
    }

    if (start >= addr + len || (start < addr && !first)) {
      break;
    }
    first = 0;

    if (kernel_copyout(vm_entry + vm_prot_off, &vm_prot, sizeof(vm_prot))) {
      return -1;
    }
    if (prot < 0) {
      prot = vm_prot;
    } else if ((prot & vm_prot) != prot) {
#if defined(__ORBIS__)
      return EINVAL;
#else
      SET_ERRNO(EINVAL);
      return -1;
#endif
    }

    if (kernel_copyout(vm_entry + 0x08, &vm_entry, sizeof(vm_entry))) {
      return -1;
    }
  }

  if (prot < 0) {
#if defined(__ORBIS__)
    return EFAULT;
#else
    SET_ERRNO(EFAULT);
#endif
  }

  return prot;
}

int kernel_set_vmem_protection(int pid, unsigned long addr, unsigned long len,
                               int prot) {
  unsigned char vm_prot = prot;
  unsigned long vm_entry;
  unsigned long start;
#if defined(__ORBIS__)
  int first = 0;
  unsigned long vm_prot_off = 0x5c;
#else
  int first = 1;
  unsigned long vm_prot_off = 0x64;
#endif

  if (prot < 0) {
#if defined(__ORBIS__)
    return EINVAL;
#else
    SET_ERRNO(EINVAL);
    return -1;
#endif
  }
#if defined(__ORBIS__)
  if (!KERNEL_OFFSET_VMSPACE_P_ROOT) {
    return ENOSYS;
  }
#endif

  if (!(vm_entry = kernel_get_vmem_entry(pid, addr))) {
    return -1;
  }

  while (vm_entry) {
    if (kernel_copyout(vm_entry + 0x20, &start, sizeof(start))) {
      return -1;
    }

    if (start >= addr + len || (start < addr && !first)) {
      break;
    }
    first = 0;

    if (kernel_copyin(&vm_prot, vm_entry + vm_prot_off, sizeof(vm_prot))) {
      return -1;
    }

    if (kernel_copyout(vm_entry + 0x08, &vm_entry, sizeof(vm_entry))) {
      return -1;
    }
  }

  return 0;
}

int kernel_mprotect(int pid, unsigned long addr, unsigned long len, int prot) {
  return kernel_set_vmem_protection(pid, addr, len, prot);
}

int kernel_proc_setchar(int pid, unsigned long addr, char val) {
  return kernel_proc_copyin(pid, &val, addr, sizeof(val));
}

int kernel_proc_setshort(int pid, unsigned long addr, short val) {
  return kernel_proc_copyin(pid, &val, addr, sizeof(val));
}

int kernel_proc_setint(int pid, unsigned long addr, int val) {
  return kernel_proc_copyin(pid, &val, addr, sizeof(val));
}

int kernel_proc_setlong(int pid, unsigned long addr, long val) {
  return kernel_proc_copyin(pid, &val, addr, sizeof(val));
}

unsigned char kernel_proc_getchar(int pid, unsigned long addr) {
  unsigned char val = 0;

  kernel_proc_copyout(pid, addr, &val, sizeof(val));

  return val;
}

unsigned short kernel_proc_getshort(int pid, unsigned long addr) {
  unsigned short val = 0;

  kernel_proc_copyout(pid, addr, &val, sizeof(val));

  return val;
}

unsigned int kernel_proc_getint(int pid, unsigned long addr) {
  unsigned int val = 0;

  kernel_proc_copyout(pid, addr, &val, sizeof(val));

  return val;
}

unsigned long kernel_proc_getlong(int pid, unsigned long addr) {
  unsigned long val = 0;

  kernel_proc_copyout(pid, addr, &val, sizeof(val));

  return val;
}
