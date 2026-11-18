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

#pragma once

#if defined(__PROSPERO__)
#include "payload.h"
#endif

/**
 * dynamic library loaded in kernel memory.
 **/
typedef struct dynlib_dynsec {
  struct {
    unsigned long le_next;
    unsigned long le_prev;
  } list_entry;

  unsigned long sysvec;
  unsigned int refcount;
  unsigned long size;

  unsigned long symtab;
  unsigned long symtabsize;

  unsigned long strtab;
  unsigned long strtabsize;

  unsigned long pltrela;
  unsigned long pltrelasize;

  unsigned long rela;
  unsigned long relasize;

  unsigned long hash;
  unsigned long hashsize;

  unsigned long dynamic;
  unsigned long dynamicsize;

  unsigned long sce_comment;
  unsigned long sce_commentsize;

  unsigned long sce_dynlib;
  unsigned long sce_dynlibsize;

  unsigned long unknown1; // execpath?
  unsigned long unknown1size;

  unsigned long buckets;
  unsigned long bucketssize;
  unsigned int nbuckets;

  unsigned long chains;
  unsigned long chainssize;
  unsigned int nchains;

  unsigned long unknown2[7];
} dynlib_dynsec_t;

/**
 * an ELF loaded in kernel memory.
 **/
typedef struct dynlib_obj {
  unsigned long next;
  unsigned long path;
  unsigned long unknown0[2];
  unsigned int refcount;
  unsigned long handle;

  unsigned long mapbase;
  unsigned long mapsize;
  unsigned long textsize;

  unsigned long database;
  unsigned long datasize;

  unsigned long unknown1;
  unsigned long unknown1size;

#if defined(__ORBIS__)
  unsigned long vaddrbase;
  unsigned long relocbase;
  unsigned long entry;
#elif defined(__PROSPERO__)
  unsigned long entry;
  unsigned long unknown2;
  unsigned long vaddrbase;
#endif

  unsigned int tlsindex;
  unsigned long tlsinit;
  unsigned long tlsinitsize;
  unsigned long tlssize;
  unsigned long tlsoffset;
  unsigned long tlsalign;

  unsigned long pltgot;

#if defined(__ORBIS__)
  unsigned long unknown2[7];
#elif defined(__PROSPERO__)
  unsigned long unknown3[6];
#endif

  unsigned long init;
  unsigned long fini;

  unsigned long eh_frame_hdr;
  unsigned long eh_frame_hdr_size;

  unsigned long eh_frame;
  unsigned long eh_frame_size;

  int status;
  int flags;

#if defined(__ORBIS__)
  unsigned long unknown5[5];

  unsigned long dynsec;
  unsigned long unknown6[6]; // fingerprint?
#elif defined(__PROSPERO__)
  unsigned long unknown4[5];

  unsigned long dynsec;
  unsigned long unknown5[6]; // fingerprint?
#endif
} dynlib_obj_t;

#if defined(__ORBIS__)
extern unsigned long KERNEL_ADDRESS_IMAGE_BASE;
extern unsigned long KERNEL_ADDRESS_COPYIN;
extern unsigned long KERNEL_ADDRESS_COPYOUT;
extern unsigned long KERNEL_ADDRESS_DMAP_BASE;
#elif defined(__PROSPERO__)
extern unsigned long KERNEL_ADDRESS_DATA_BASE;
extern unsigned long KERNEL_ADDRESS_TEXT_BASE;
extern unsigned long KERNEL_ADDRESS_SECURITY_FLAGS;
extern unsigned long KERNEL_ADDRESS_BUS_DATA_DEVICES;
extern unsigned long KERNEL_ADDRESS_DMAP_BASE;
extern unsigned long KERNEL_ADDRESS_IOMMU_SOFTC;
extern unsigned long KERNEL_ADDRESS_UTOKEN_FLAGS;
#endif

extern unsigned long KERNEL_ADDRESS_ALLPROC;
extern unsigned long KERNEL_ADDRESS_ROOTVNODE;
extern unsigned long KERNEL_ADDRESS_PRISON0;
extern unsigned long KERNEL_ADDRESS_QA_FLAGS;
extern unsigned long KERNEL_ADDRESS_TARGETID;

extern const unsigned long KERNEL_OFFSET_PROC_P_UCRED;
extern const unsigned long KERNEL_OFFSET_PROC_P_FD;
extern const unsigned long KERNEL_OFFSET_PROC_P_PID;
extern const unsigned long KERNEL_OFFSET_PROC_P_VMSPACE;
extern const unsigned long KERNEL_OFFSET_PROC_P_DYNLIB;

extern const unsigned long KERNEL_OFFSET_THREAD_TD_TID;

extern const unsigned long KERNEL_OFFSET_UCRED_CR_UID;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_RUID;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_SVUID;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_RGID;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_SVGID;

extern const unsigned long KERNEL_OFFSET_UCRED_CR_PRISON;

extern const unsigned long KERNEL_OFFSET_UCRED_CR_SCEAUTHID;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_SCECAPS;
extern const unsigned long KERNEL_OFFSET_UCRED_CR_SCEATTRS;

extern const unsigned long KERNEL_OFFSET_FILEDESC_FD_CDIR;
extern const unsigned long KERNEL_OFFSET_FILEDESC_FD_RDIR;
extern const unsigned long KERNEL_OFFSET_FILEDESC_FD_JDIR;

extern unsigned long KERNEL_OFFSET_VMSPACE_P_ROOT;
#if defined(__PROSPERO__)
extern unsigned long KERNEL_OFFSET_VMSPACE_VM_PMAP;
#endif

#if defined(__ORBIS__)
extern unsigned long KERNEL_IMAGE_SIZE;
#endif

/**
 * function in libkernel that returns a pointer to errno. Resolved by
 * __kernel_init() on PS5; stays unresolved (NULL) on PS4, so SET_ERRNO is
 * a no-op there, same as before kernel_dynlib_* and kernel_get_proc_thread()
 * moved into shared/kernel.c.
 **/
extern int *(*sdk__error)(void);

#define SET_ERRNO(val)                                                        \
  if (sdk__error) {                                                              \
    *sdk__error() = val;                                                        \
  }

int init_offsets(int fw_version);

unsigned int kernel_get_fw_version(void);

int kernel_copyout(unsigned long kaddr, void *uaddr, unsigned long len);
int kernel_copyin(const void *uaddr, unsigned long kaddr, unsigned long len);

int kernel_setlong(unsigned long addr, unsigned long val);
int kernel_setint(unsigned long addr, unsigned int val);
int kernel_setshort(unsigned long addr, unsigned short val);
int kernel_setchar(unsigned long addr, unsigned char val);

unsigned long kernel_getlong(unsigned long addr);
unsigned int kernel_getint(unsigned long addr);
unsigned short kernel_getshort(unsigned long addr);
unsigned char kernel_getchar(unsigned long addr);

unsigned long kernel_get_proc(int pid);
unsigned long kernel_get_proc_thread(int pid, int tid);
unsigned long kernel_get_proc_ucred(int pid);
unsigned long kernel_get_proc_filedesc(int pid);
unsigned long kernel_get_proc_file(int pid, int fd);

#if defined(__PROSPERO__)
int kernel_proc_getpaddr(int pid, unsigned long vaddr, unsigned long *paddr,
                         unsigned long *plen);
#endif

int kernel_proc_copyin(int pid, const void *buf, unsigned long addr,
                       unsigned long len);
int kernel_proc_copyout(int pid, unsigned long addr, void *buf,
                        unsigned long len);

int kernel_proc_setchar(int pid, unsigned long addr, char val);
int kernel_proc_setshort(int pid, unsigned long addr, short val);
int kernel_proc_setint(int pid, unsigned long addr, int val);
int kernel_proc_setlong(int pid, unsigned long addr, long val);

unsigned char kernel_proc_getchar(int pid, unsigned long addr);
unsigned short kernel_proc_getshort(int pid, unsigned long addr);
unsigned int kernel_proc_getint(int pid, unsigned long addr);
unsigned long kernel_proc_getlong(int pid, unsigned long addr);

#if defined(__PROSPERO__)
int kernel_iommu_copyin(const void *uaddr, unsigned long kaddr,
                        unsigned long len);

int kernel_iommu_setlong(unsigned long kaddr, unsigned long val);
int kernel_iommu_setint(unsigned long kaddr, unsigned int val);
int kernel_iommu_setshort(unsigned long kaddr, unsigned short val);
int kernel_iommu_setchar(unsigned long kaddr, unsigned char val);
#endif

unsigned long kernel_get_ucred_authid(int pid);
int kernel_set_ucred_authid(int pid, unsigned long authid);

int kernel_get_ucred_attrs(int pid, unsigned char attrs[32]);
int kernel_set_ucred_attrs(int pid, const unsigned char attrs[32]);

int kernel_get_ucred_caps(int pid, unsigned char caps[16]);
int kernel_set_ucred_caps(int pid, const unsigned char caps[16]);

int kernel_get_ucred_uid(int pid);
int kernel_set_ucred_uid(int pid, unsigned int uid);
int kernel_get_ucred_ruid(int pid);
int kernel_set_ucred_ruid(int pid, unsigned int ruid);
int kernel_get_ucred_svuid(int pid);
int kernel_set_ucred_svuid(int pid, unsigned int svuid);
int kernel_get_ucred_rgid(int pid);
int kernel_set_ucred_rgid(int pid, unsigned int rgid);
int kernel_get_ucred_svgid(int pid);
int kernel_set_ucred_svgid(int pid, unsigned int svgid);

unsigned long kernel_get_ucred_prison(int pid);
int kernel_set_ucred_prison(int pid, unsigned long prison);

unsigned long kernel_get_root_vnode(void);
unsigned long kernel_get_proc_rootdir(int pid);
int kernel_set_proc_rootdir(int pid, unsigned long vnode);
unsigned long kernel_get_proc_jaildir(int pid);
int kernel_set_proc_jaildir(int pid, unsigned long vnode);
unsigned long kernel_get_proc_currdir(int pid);
int kernel_set_proc_currdir(int pid, unsigned long vnode);

int kernel_dynlib_obj(int pid, unsigned int handle, dynlib_obj_t *obj);
int kernel_dynlib_handle(int pid, const char *basename, unsigned int *handle);
int kernel_dynlib_find_handle(int pid, unsigned long addr,
                              unsigned int *handle);
unsigned long kernel_dynlib_mapbase_addr(int pid, unsigned int handle);
unsigned long kernel_dynlib_entry_addr(int pid, unsigned int handle);
unsigned long kernel_dynlib_init_addr(int pid, unsigned int handle);
unsigned long kernel_dynlib_fini_addr(int pid, unsigned int handle);
unsigned long kernel_dynlib_resolve(int pid, int handle, const char *nid);
unsigned long kernel_dynlib_dlsym(int pid, unsigned int handle,
                                  const char *sym);
int kernel_dynlib_path(int pid, unsigned int handle, char *path,
                       unsigned long size);

#if defined(__ORBIS__)
unsigned long kernel_find_pattern(unsigned long addr, unsigned int len,
                                  const char *pattern);
#endif
int kernel_patch(unsigned long addr, const void *before, const void *after,
                 unsigned short size);

int kernel_get_qaflags(unsigned char qaflags[16]);
int kernel_set_qaflags(const unsigned char qaflags[16]);

unsigned long kernel_get_vmem_entry(int pid, unsigned long addr);
int kernel_get_vmem_protection(int pid, unsigned long addr, unsigned long len);
int kernel_set_vmem_protection(int pid, unsigned long addr, unsigned long len,
                               int prot);
int kernel_mprotect(int pid, unsigned long addr, unsigned long size, int prot);

#if defined(__PROSPERO__)
int kernel_overlap_sockets(int pid, int master_sock, int victim_sock);
#endif

#if defined(__ORBIS__)
int __kernel_init(void);
#elif defined(__PROSPERO__)
int __kernel_init(payload_args_t *args);
#endif
