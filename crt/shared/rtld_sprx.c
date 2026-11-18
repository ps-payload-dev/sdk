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

#include "rtld_sprx.h"
#include "elf.h"
#include "kernel.h"
#include "klog.h"
#include "nid.h"
#include "syscall.h"

/**
 * Dependencies to standard libraries.
 **/
static char *(*strcpy)(char *, const char *) = 0;
static int (*strcmp)(const char *, const char *) = 0;
static int (*strncmp)(const char *, const char *, unsigned long) = 0;

static void *(*calloc)(unsigned long, unsigned long) = 0;
static void *(*malloc)(unsigned long) = 0;
static void (*free)(void *) = 0;

static int (*sceKernelLoadStartModule)(const char *, unsigned long,
                                       const void *, unsigned int, void *,
                                       int *) = 0;
static int (*sceKernelStopUnloadModule)(int, unsigned long, const void *,
                                        unsigned int, const void *, void *) = 0;
static int (*sceSysmoduleLoadModuleInternal)(unsigned int) = 0;

/**
 * Extend the rtld_lib_t type with auxiliary parameters needed by .sprx loader.
 * This requires -fms-extensions.
 **/
typedef struct rtld_sprx_lib {
  struct rtld_lib;

  int handle;
  char *strtab;
  Elf64_Sym *symtab;
  unsigned long symtab_size;

  int unload_on_close;
} rtld_sprx_lib_t;

/**
 * Open up a new sprx object and initialize its members.
 **/
static int sprx_open(rtld_lib_t *ctx) {
  rtld_sprx_lib_t *lib = (rtld_sprx_lib_t *)ctx;
  unsigned int handle = 0;
  unsigned int sysmod = 0;
  dynlib_dynsec_t dynsec;
  dynlib_obj_t obj;
  char path[1024];
  int unload = 0;
  int error = 0;
  int ret = 0;

  // check if the lib is an alias for libkernel
  if (!strcmp(lib->soname, "libkernel.sprx") ||
      !strcmp(lib->soname, "libkernel_web.sprx") ||
      !strcmp(lib->soname, "libkernel_sys.sprx")) {
    if (kernel_dynlib_dlsym(-1, 0x1, "sceKernelDlsym")) {
      handle = 1;
    } else {
      handle = 0x2001;
    }
  }
  // check if lib is already opened
  else if (kernel_dynlib_handle(-1, lib->soname, &handle) < 0) {
    if (__rtld_find_file(lib->soname, path)) {
      klog_printf("Unable to find '%s'\n", lib->soname);
      return -1;
    }

    if ((sysmod = __rtld_sysmodule_id(lib->soname))) {
      if ((error = sceSysmoduleLoadModuleInternal(sysmod))) {
        klog_printf("sceSysmoduleLoadModuleInternal: 0x%x\n", error);
        return -1;
      }
    }

    if ((ret = sceKernelLoadStartModule(path, 0, 0, 0, 0, 0)) < 0) {
      klog_printf("sceKernelLoadStartModule: 0x%x\n", ret);
      return -1;
    }
    handle = ret;
    unload = 1;
  }

  lib->handle = handle;
  lib->unload_on_close = unload;

  if (kernel_dynlib_obj(-1, handle, &obj) < 0) {
    error = 1;
  } else if (kernel_copyout(obj.path, lib->soname, 1024) < 0) {
    error = 1;
  } else if (kernel_copyout(obj.dynsec, &dynsec, sizeof(dynsec)) < 0) {
    error = 1;
  } else if (!(lib->strtab = malloc(dynsec.strtabsize))) {
    klog_perror("malloc");
    return -1;
  } else if (kernel_copyout(dynsec.strtab, lib->strtab, dynsec.strtabsize) <
             0) {
    error = 1;
  } else if (!(lib->symtab = malloc(dynsec.symtabsize))) {
    klog_perror("malloc");
    return -1;
  } else if (kernel_copyout(dynsec.symtab, lib->symtab, dynsec.symtabsize) <
             0) {
    error = 1;
  }

  if (error) {
    klog_puts("Unknown kernel I/O error");
    return -1;
  }

  lib->mapbase = (void *)obj.mapbase;
  lib->mapsize = obj.mapsize;
  lib->symtab_size = dynsec.symtabsize;

  return 0;
}

/**
 * TODO: sprx_init
 **/
static int sprx_init(rtld_lib_t *ctx, int argc, char **argv, char **envp) {
  return 0;
}

/**
 * TODO: sprx_fini
 **/
static int sprx_fini(rtld_lib_t *ctx) { return 0; }

/**
 * Resolve the address of a symbol provided by the given sprx object-
 **/
static void *sprx_sym2addr(rtld_lib_t *ctx, const char *name) {
  rtld_sprx_lib_t *lib = (rtld_sprx_lib_t *)ctx;
  char nid[12];

  if (!lib->symtab || !lib->strtab || !lib->mapbase) {
    return 0;
  }

  nid_encode(name, nid);

  for (unsigned long i = 0; i < lib->symtab_size / sizeof(Elf64_Sym); i++) {
    if (lib->symtab[i].st_shndx == SHN_UNDEF) {
      continue;
    }

    if (!strncmp(nid, lib->strtab + lib->symtab[i].st_name, 11) ||
        !strncmp(name, lib->strtab + lib->symtab[i].st_name, 11)) {
      return lib->mapbase + lib->symtab[i].st_value;
    }
  }

  return 0;
}

/**
 * Figure out the symbol name associated with the given address.
 **/
static const char *sprx_addr2sym(rtld_lib_t *ctx, void *addr) {
  rtld_sprx_lib_t *lib = (rtld_sprx_lib_t *)ctx;

  if (!lib->symtab || !lib->strtab || !lib->mapbase) {
    return 0;
  }

  if (addr < lib->mapbase || addr >= lib->mapbase + lib->mapsize) {
    return 0;
  }

  for (unsigned long i = 0; i < lib->symtab_size / sizeof(Elf64_Sym); i++) {
    if (!lib->symtab[i].st_size) {
      continue;
    }

    if (addr >= lib->mapbase + lib->symtab[i].st_value &&
        addr <
            lib->mapbase + lib->symtab[i].st_value + lib->symtab[i].st_size) {
      return lib->strtab + lib->symtab[i].st_name;
    }
  }

  return 0;
}

/**
 * Close the given sprx object and free memory associated with its members.
 **/
static int sprx_close(rtld_lib_t *ctx) {
  rtld_sprx_lib_t *lib = (rtld_sprx_lib_t *)ctx;
  int err = 0;

  if (lib->unload_on_close) {
    if ((err = sceKernelStopUnloadModule(lib->handle, 0, 0, 0, 0, 0))) {
      return err;
    }
  }

  if (lib->strtab) {
    free(lib->strtab);
  }
  if (lib->symtab) {
    free(lib->symtab);
  }

  lib->handle = -1;
  lib->strtab = 0;
  lib->symtab = 0;
  lib->symtab_size = 0;
  lib->mapbase = 0;
  lib->mapsize = 0;
  lib->unload_on_close = 0;

  return 0;
}

/**
 * Free all memory associated the given sprx object.
 **/
static void sprx_destroy(rtld_lib_t *ctx) {
  rtld_sprx_lib_t *lib = (rtld_sprx_lib_t *)ctx;

  if (sprx_close(ctx)) {
    lib->unload_on_close = 0;
    sprx_close(ctx);
  }

  free(lib);
}

rtld_lib_t *__rtld_sprx_new(rtld_lib_t *parent, const char *soname) {
  rtld_sprx_lib_t *lib = calloc(1, sizeof(rtld_sprx_lib_t));

  if (!lib) {
    return 0;
  }

  lib->parent = parent;
  lib->open = sprx_open;
  lib->init = sprx_init;
  lib->sym2addr = sprx_sym2addr;
  lib->addr2sym = sprx_addr2sym;
  lib->fini = sprx_fini;
  lib->close = sprx_close;
  lib->destroy = sprx_destroy;
  lib->refcnt = 0;

  __rtld_strlcpy(lib->soname, soname, sizeof(lib->soname));

  return (rtld_lib_t *)lib;
}

int __rtld_sprx_init(void) {
  unsigned int libSceSysmodule;
  unsigned int libkernel = 0x1;
  int ret;
  unsigned int libc = 0x2;

  if (!kernel_dynlib_dlsym(-1, libkernel, "sceKernelDlsym")) {
    libkernel = 0x2001;
  }
  if (!KERNEL_DLSYM(libkernel, sceKernelLoadStartModule)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libkernel, sceKernelStopUnloadModule)) {
    return -1;
  }

  if (!KERNEL_DLSYM(libc, strcpy)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libc, strcmp)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libc, strncmp)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libc, calloc)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libc, malloc)) {
    return -1;
  }
  if (!KERNEL_DLSYM(libc, free)) {
    return -1;
  }

  if (kernel_dynlib_handle(-1, "libSceSysmodule.sprx", &libSceSysmodule)) {
    if ((ret = sceKernelLoadStartModule(
             "/system/common/lib/libSceSysmodule.sprx", 0, 0, 0, 0, 0)) <= 0) {
      return -1;
    }
    libSceSysmodule = ret;
  }
  if (!KERNEL_DLSYM(libSceSysmodule, sceSysmoduleLoadModuleInternal)) {
    return -1;
  }

  return 0;
}
