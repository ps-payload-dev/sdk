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

#include "rtld.h"
#include "elf.h"
#include "kernel.h"
#include "klog.h"
#include "syscall.h"

#include "rtld_dlfcn.h"
#include "rtld_payload.h"
#include "rtld_so.h"
#include "rtld_sprx.h"

/**
 * Dependencies to standard libraries.
 **/
static int (*strcmp)(const char *, const char *) = 0;
static int (*sprintf)(char *, const char *, ...) = 0;

static char *(*strcpy)(char *, const char *) = 0;
static int (*strncmp)(const char *, const char *, unsigned long) = 0;
static unsigned long (*strlen)(const char *) = 0;
static char *(*strcat)(char *, const char *) = 0;

static char *(*getenv)(const char *) = 0;

static void *(*calloc)(unsigned long, unsigned long) = 0;
static void (*free)(void *) = 0;

static const struct sysmodtab {
  const char *name;
  unsigned int handle;
} sysmodtab[] = {
    {"libSceAppInstUtil.sprx", 0x80000014},
    {"libSceHttp2.sprx", 0x8000008c},
    {"libSceNet.sprx", 0x8000001c},
    {"libSceSsl.sprx", 0x8000000b},
    {"libSceSystemService.sprx", 0x80000010},
    {"libSceUserService.sprx", 0x80000011},
#if defined(__PROSPERO__)
    {"libSceAbstractLocal.sprx", 0x8000005f},
    {"libSceAbstractStorage.sprx", 0x80000058},
    {"libSceAbstractTcs.sprx", 0x800000a1},
    {"libSceAbstractTwitter.sprx", 0x80000062},
    {"libSceAbstractYoutube.sprx", 0x80000061},
    {"libSceAc3Enc.sprx", 0x80000013},
    {"libSceAgc.sprx", 0x80000094},
    {"libSceAgcDriver.sprx", 0x80000080},
    {"libSceAgcResourceRegistration.sprx", 0x80000093},
    {"libSceAgcVsh.sprx", 0x80000086},
    {"libSceAgcVshDebug.sprx", 0x80000087},
    {"libSceAjm.native.sprx", 0x80000023},
    {"libSceAjmi.sprx", 0x8000007e},
    {"libSceAmpr.sprx", 0x800000b7},
    {"libSceAppChecker.sprx", 0x80000032},
    {"libSceAppDbShellCoreClient.sprx", 0x800000a7},
    {"libSceAsyncStorageInternal.sprx", 0x80000077},
    {"libSceAudioIn.sprx", 0x80000002},
    {"libSceAudioOut.sprx", 0x80000001},
    {"libSceAudioSystem.sprx", 0x80000083},
    {"libSceAudiodecCpuDtsHdMa.sprx", 0x8000002d},
    {"libSceAudiodecCpuLpcm.sprx", 0x8000002e},
    {"libSceAudiodecCpuTrhd.sprx", 0x80000082},
    {"libSceAvSetting.sprx", 0x80000021},
    {"libSceAvcap2.sprx", 0x80000085},
    {"libSceBackupRestoreUtil.sprx", 0x8000003f},
    {"libSceBgft.sprx", 0x8000002a},
    {"libSceBgsStorage.sprx", 0x800000a3},
    {"libSceCamera.sprx", 0x8000001a},
    {"libSceCdlgUtilServer.sprx", 0x80000007},
    {"libSceCommonDialog.sprx", 0x80000018},
    {"libSceComposite.sprx", 0x8000008a},
    {"libSceCompositeExt.sprx", 0x8000008b},
    {"libSceContentListController.sprx", 0x800000ad},
    {"libSceDataTransfer.sprx", 0x80000057},
    {"libSceDbg.sprx", 0x80000025},
    {"libSceDipsw.sprx", 0x80000029},
    {"libSceDseehx.sprx", 0x80000056},
    {"libSceDtsEnc.sprx", 0x80000028},
    {"libSceEmbeddedTts.sprx", 0x8000009c},
    {"libSceEmbeddedTtsCoreG3.sprx", 0x8000009b},
    {"libSceFsInternalForVsh.sprx", 0x80000066},
    {"libSceGLSlimVSH.sprx", 0x800000a9},
    {"libSceGifParser.sprx", 0x8000005e},
    {"libSceGpuCapture.sprx", 0x8000007f},
    {"libSceGpuTrace.sprx", 0x8000007b},
    {"libSceGvMp4Parser.sprx", 0x8000005c},
    {"libSceHidControl.sprx", 0x80000017},
    {"libSceHttp.sprx", 0x8000000a},
    {"libSceHttpCache.sprx", 0x80000078},
    {"libSceIcu.sprx", 0x800000a8},
    {"libSceIdu.sprx", 0x800000a6},
    {"libSceImageUtil.sprx", 0x80000059},
    {"libSceIpmi.sprx", 0x8000001d},
    {"libSceJemspace.sprx", 0x8000009e},
    {"libSceJitBridge.sprx", 0x8000006f},
    {"libSceJpegParser.sprx", 0x8000005b},
    {"libSceJsc.sprx", 0x800000b0},
    {"libSceJscCompiler.sprx", 0x80000070},
    {"libSceJxr.sprx", 0x800000b4},
    {"libSceJxrParser.sprx", 0x800000b5},
    {"libSceKbEmulate.sprx", 0x80000031},
    {"libSceLibreSsl.sprx", 0x80000065},
    {"libSceLibreSsl3.sprx", 0x800000b8},
    {"libSceLoginMgrServer.sprx", 0x80000045},
    {"libSceMarlin.sprx", 0x80000027},
    {"libSceMat.sprx", 0x80000048},
    {"libSceMbus.sprx", 0x8000001e},
    {"libSceMediaFrameworkInterface.sprx", 0x80000095},
    {"libSceMediaFrameworkUtil.sprx", 0x800000b6},
    {"libSceMetadataReaderWriter.sprx", 0x8000005a},
    {"libSceNKWeb.sprx", 0x80000079},
    {"libSceNKWebKit.sprx", 0x8000007a},
    {"libSceNetCtl.sprx", 0x80000009},
    {"libSceNgs2.sprx", 0x80000090},
    {"libSceNpCommon.sprx", 0x8000000c},
    {"libSceNpGameIntent.sprx", 0x8000008d},
    {"libSceNpManager.sprx", 0x8000000d},
    {"libSceNpRemotePlaySessionSignaling.sprx", 0x8000009a},
    {"libSceNpSns.sprx", 0x8000001b},
    {"libSceNpTcs.sprx", 0x800000a0},
    {"libSceNpWebApi.sprx", 0x8000000e},
    {"libSceNpWebApi2.sprx", 0x8000008f},
    {"libSceOpusCeltDec.sprx", 0x80000044},
    {"libSceOpusCeltEnc.sprx", 0x80000043},
    {"libSceOpusDec.sprx", 0x80000069},
    {"libSceOpusSilkEnc.sprx", 0x80000068},
    {"libSceOrbisCompat.sprx", 0x80000071},
    {"libScePad.sprx", 0x80000024},
    {"libScePngParser.sprx", 0x8000005d},
    {"libScePosixForWebKit.sprx", 0x80000098},
    {"libScePsm.sprx", 0x80000030},
    {"libSceRazorCpu_debug.sprx", 0x80000075},
    {"libSceRegMgr.sprx", 0x8000001f},
    {"libSceRemotePlayClientIpc.sprx", 0x800000b9},
    {"libSceResourceArbitrator.sprx", 0x80000092},
    {"libSceRnpsAppMgr.sprx", 0x80000076},
    {"libSceRtc.sprx", 0x80000020},
    {"libSceSaveData.sprx", 0x8000000f},
    {"libSceShareInternal.native.sprx", 0x8000008e},
    {"libSceSocialScreen.sprx", 0x800000ae},
    {"libSceSulphaDrv.sprx", 0x8000003b},
    {"libSceSysCore.sprx", 0x80000004},
    {"libSceSysUtil.sprx", 0x80000026},
    {"libSceSystemLogger2.sprx", 0x800000b3},
    {"libSceSystemLogger2Delivery.sprx", 0x80000089},
    {"libSceSystemLogger2Game.sprx", 0x8000009f},
    {"libSceSystemLogger2NativeQueueClient.sprx", 0x80000088},
    {"libSceSystemTts.sprx", 0x80000097},
    {"libSceTEEClient.sprx", 0x800000a2},
    {"libSceVcodec.sprx", 0x80000091},
    {"libSceVdecCore.native.sprx", 0x80000015},
    {"libSceVdecSavc2.native.sprx", 0x80000036},
    {"libSceVdecShevc.native.sprx", 0x8000003c},
    {"libSceVdecSvp9.native.sprx", 0x800000af},
    {"libSceVenc.sprx", 0x80000084},
    {"libSceVideoOut.sprx", 0x80000022},
    {"libSceVideoOutSecondary.sprx", 0x80000046},
    {"libSceVideoStreamingEngine_sys.sprx", 0x800000b2},
    {"libSceVisionManager.sprx", 0x80000012},
    {"libSceVnaInternal.sprx", 0x8000007c},
    {"libSceVnaWebsocket.sprx", 0x8000007d},
    {"libSceVoiceCommand.sprx", 0x80000099},
    {"libSceWeb.sprx", 0x80000072},
    {"libSceWebKit2.sprx", 0x80000073},
    {"libSceWebKit2Secure.sprx", 0x80000074},
    {"libSceWebmParserMdrw.sprx", 0x800000a4},
    {"libcairo.sprx", 0x800000ac},
    {"libcurl.sprx", 0x800000b1},
    {"libicu.sprx", 0x800000aa},
    {"libpng16.sprx", 0x800000ab},
#endif
};

unsigned int __rtld_sysmodule_id(const char *soname) {
  for (unsigned long i = 0; i < sizeof(sysmodtab) / sizeof(sysmodtab[0]); i++) {
    if (!strcmp(soname, sysmodtab[i].name)) {
      return sysmodtab[i].handle;
    }
  }

  return 0;
}

unsigned long __rtld_strlcpy(char *dst, const char *src, unsigned long size) {
  unsigned long len = 0;

  while (src[len]) {
    len++;
  }

  if (size) {
    unsigned long n = len < size - 1 ? len : size - 1;

    for (unsigned long i = 0; i < n; i++) {
      dst[i] = src[i];
    }
    dst[n] = 0;
  }

  return len;
}

/**
 * Extend the rtld_lib_t type with auxiliary parameters for shallow
 * copies of a lib. This requires -fms-extensions.
 **/
typedef struct rtld_ref_lib {
  struct rtld_lib;

  rtld_lib_t *ref;
} rtld_ref_lib_t;

/**
 * Check if a string ends with a given suffix.
 **/
static int endswith(const char *string, const char *suffix) {
  unsigned long suffix_len = strlen(suffix);
  unsigned long string_len = strlen(string);

  if (string_len < suffix_len) {
    return 0;
  }

  return strncmp(string + string_len - suffix_len, suffix, suffix_len) == 0;
}

/**
 *
 **/
static int ref_open(rtld_lib_t *ctx) {
  rtld_ref_lib_t *lib = (rtld_ref_lib_t *)ctx;
  return __rtld_lib_open(lib->ref);
}

/**
 *
 **/
static int ref_init(rtld_lib_t *ctx, int argc, char **argv, char **envp) {
  return 0;
}

/**
 *
 **/
static void *ref_sym2addr(rtld_lib_t *ctx, const char *name) {
  rtld_ref_lib_t *lib = (rtld_ref_lib_t *)ctx;
  return __rtld_lib_sym2addr(lib->ref, name);
}

/**
 *
 **/
static const char *ref_addr2sym(rtld_lib_t *ctx, void *addr) {
  rtld_ref_lib_t *lib = (rtld_ref_lib_t *)ctx;
  return __rtld_lib_addr2sym(lib->ref, addr);
}

/**
 *
 **/
static int ref_fini(rtld_lib_t *ctx) { return 0; }

/**
 *
 **/
static int ref_close(rtld_lib_t *ctx) {
  rtld_ref_lib_t *lib = (rtld_ref_lib_t *)ctx;
  return __rtld_lib_close(lib->ref);
}

/**
 *
 **/
static void ref_destroy(rtld_lib_t *ctx) { free(ctx); }

static rtld_lib_t *__rtld_lib_soname2lib(rtld_lib_t *ctx, const char *soname) {
  rtld_lib_t *lib;

  if (!ctx) {
    return 0;
  }

  if (!strcmp(ctx->soname, soname)) {
    return ctx;
  }

  for (rtld_lib_seq_t *it = ctx->children.head; it; it = it->next) {
    if ((lib = __rtld_lib_soname2lib(it->lib, soname))) {
      return lib;
    }
  }

  return 0;
}

static int find_in_dir(char *path, const char *dir, const char *name) {
  char buf[0x100];

  if (strlen(dir) + 1 + strlen(name) >= 1024) {
    return -1;
  }

  sprintf(path, "%s/%s", dir, name);

  return (int)ps_syscall(SYS_stat, path, buf);
}

int __rtld_find_file(const char *name, char *path) {
#if defined(__PROSPERO__)
  static const char *const subdirs[] = {"priv", "common", "priv_ex",
                                        "common_ex"};
  unsigned long random_word_len = 0xff;
  char random_word[0xff];
#endif
  const char *ldpaths;
  char dir[1024];
  char buf[0x100];
  int err = 0;

  if (!name || !*name || !path) {
    return -1;
  }

  path[0] = 0;
  if (name[0] == '/') {
    if (strlen(name) >= 1024) {
      return -1;
    }
    if (!(err = ps_syscall(SYS_stat, name, buf))) {
      __rtld_strlcpy(path, name, 1024);
    }
    return err;
  }

  if (strlen(name) > 512) {
    return -1;
  }

  if (!find_in_dir(path, "/system/priv/lib", name) ||
      !find_in_dir(path, "/system/common/lib", name) ||
      !find_in_dir(path, "/system_ex/priv_ex/lib", name) ||
      !find_in_dir(path, "/system_ex/common_ex/lib", name)) {
    return 0;
  }

#if defined(__PROSPERO__)
  if (!ps_syscall(SYS_randomized_path, 0, &random_word, &random_word_len)) {
    random_word[sizeof(random_word) - 1] = 0;

    for (int i = 0; i < 4; i++) {
      sprintf(dir, "/%s/%s/lib", random_word, subdirs[i]);
      if (!find_in_dir(path, dir, name)) {
        return 0;
      }
    }
  }
#endif

  if ((ldpaths = getenv("LD_LIBRARY_PATH"))) {
    while (*ldpaths) {
      unsigned long len = 0;

      while (ldpaths[len] && ldpaths[len] != ':') {
        len++;
      }

      if (len && len < sizeof(dir)) {
        __rtld_strlcpy(dir, ldpaths, len + 1);
        if (!find_in_dir(path, dir, name)) {
          return 0;
        }
      }

      ldpaths += len;
      if (*ldpaths == ':') {
        ldpaths++;
      }
    }
  }

  if (!find_in_dir(path, "/user/homebrew/lib", name)) {
    return 0;
  }

  if (!ps_syscall(SYS___getcwd, dir, sizeof(dir))) {
    if (!find_in_dir(path, dir, name)) {
      return 0;
    }
  }

  path[0] = 0;
  return -1;
}

#if defined(__ORBIS__)
static struct {
  unsigned long rootdir;
  unsigned long jaildir;
  unsigned long prison;
  int depth;
  int dirty;
} jail;

static int rtld_jail_restore(void) {
  int err = 0;

  if (kernel_set_proc_rootdir(-1, jail.rootdir)) {
    klog_puts("kernel_set_proc_rootdir failed");
    err = -1;
  }
  if (kernel_set_proc_jaildir(-1, jail.jaildir)) {
    klog_puts("kernel_set_proc_jaildir failed");
    err = -1;
  }
  if (kernel_set_ucred_prison(-1, jail.prison)) {
    klog_puts("kernel_set_ucred_prison failed");
    err = -1;
  }

  jail.dirty = err ? 1 : 0;

  return err;
}

static int rtld_jail_enter(void) {
  int err;

  if (jail.depth) {
    jail.depth++;
    return 0;
  }

  if (!jail.dirty) {
    if (!(jail.prison = kernel_get_ucred_prison(-1))) {
      klog_puts("kernel_get_ucred_prison failed");
      return -1;
    }
    if (!(jail.rootdir = kernel_get_proc_rootdir(-1))) {
      klog_puts("kernel_get_proc_rootdir failed");
      return -1;
    }
    if (!(jail.jaildir = kernel_get_proc_jaildir(-1))) {
      klog_puts("kernel_get_proc_jaildir failed");
      return -1;
    }
  }

  if ((err = kernel_set_proc_rootdir(-1, KERNEL_ADDRESS_ROOTVNODE))) {
    klog_puts("kernel_set_proc_rootdir failed");
  } else if ((err = kernel_set_proc_jaildir(-1, KERNEL_ADDRESS_ROOTVNODE))) {
    klog_puts("kernel_set_proc_jaildir failed");
  } else if ((err = kernel_set_ucred_prison(-1, KERNEL_ADDRESS_PRISON0))) {
    klog_puts("kernel_set_ucred_prison failed");
  }

  if (err) {
    rtld_jail_restore();
    return -1;
  }

  jail.depth = 1;

  return 0;
}

static void rtld_jail_leave(void) {
  if (jail.depth && !--jail.depth) {
    rtld_jail_restore();
  }
}

#define RTLD_JAIL_ENTER() rtld_jail_enter()
#define RTLD_JAIL_LEAVE() rtld_jail_leave()
#else
#define RTLD_JAIL_ENTER() 0
#define RTLD_JAIL_LEAVE() ((void)0)
#endif

static rtld_lib_t *rtld_lib_new(rtld_lib_t *parent, const char *soname);
static int rtld_lib_open(rtld_lib_t *ctx);

rtld_lib_t *__rtld_lib_new(rtld_lib_t *parent, const char *soname) {
  rtld_lib_t *lib;

  if (RTLD_JAIL_ENTER()) {
    return 0;
  }
  lib = rtld_lib_new(parent, soname);
  RTLD_JAIL_LEAVE();

  return lib;
}

int __rtld_lib_open(rtld_lib_t *ctx) {
  int err;

  if (RTLD_JAIL_ENTER()) {
    return -1;
  }
  err = rtld_lib_open(ctx);
  RTLD_JAIL_LEAVE();

  return err;
}

static rtld_lib_t *rtld_lib_new(rtld_lib_t *parent, const char *soname) {
  rtld_lib_t *root = parent;
  rtld_ref_lib_t *lib = 0;
  rtld_lib_t *ref = 0;
  char path[1024];

  // find the root node
  while (root && root->parent) {
    root = root->parent;
  }

  if (__rtld_find_file(soname, path)) {
    __rtld_strlcpy(path, soname, sizeof(path));
  }

  // check if the lib is already loaded
  for (rtld_lib_t *it = parent; it && !ref; it = it->parent) {
    if (!strcmp(it->soname, path) || !strcmp(it->soname, soname)) {
      ref = it;
    }
  }

  if (ref || (ref = __rtld_lib_soname2lib(root, path))) {
    if (!(lib = calloc(1, sizeof(rtld_ref_lib_t)))) {
      return 0;
    }

    lib->parent = parent;
    lib->ref = ref;
    lib->open = ref_open;
    lib->init = ref_init;
    lib->sym2addr = ref_sym2addr;
    lib->addr2sym = ref_addr2sym;
    lib->fini = ref_fini;
    lib->close = ref_close;
    lib->destroy = ref_destroy;
    lib->refcnt = 0;
    lib->mapbase = ref->mapbase;
    lib->mapsize = ref->mapsize;

    strcpy(lib->soname, ref->soname);

    return (rtld_lib_t *)lib;
  }

  // .sprx and .so files have different loaders
  if (endswith(soname, ".sprx")) {
    return __rtld_sprx_new(parent, soname);
  } else {
    return __rtld_so_new(parent, soname);
  }
}

static int rtld_lib_close_deps(rtld_lib_t *ctx, int keep_going) {
  int first = 0;
  int err;

  for (rtld_lib_seq_t *it = ctx->children.tail, *prev; it; it = prev) {
    rtld_lib_t *child = it->lib;

    prev = it->prev;

    if ((err = __rtld_lib_close(child))) {
      if (!keep_going) {
        return err;
      }
      if (!first) {
        first = err;
      }
      continue;
    }

    if (child->refcnt <= 0) {
      __rtld_lib_destroy(child);
    }
  }

  return first;
}

static int rtld_lib_open(rtld_lib_t *ctx) {
  int err;

  if (!ctx) {
    return -1;
  }

  ctx->refcnt++;
  if (ctx->refcnt == 1) {
    if ((err = ctx->open(ctx))) {
      rtld_lib_close_deps(ctx, 1);
      ctx->refcnt--;
      return err;
    }
  }

  return 0;
}

int __rtld_lib_append_dep(rtld_lib_t *ctx, rtld_lib_t *lib) {
  rtld_lib_seq_t *seq;

  if (!ctx || !lib) {
    return -1;
  }

  if (!(seq = calloc(1, sizeof(rtld_lib_seq_t)))) {
    return -1;
  }

  seq->lib = lib;

  if (!ctx->children.head) {
    ctx->children.head = seq;
  }

  if (!ctx->children.tail) {
    ctx->children.tail = seq;
  } else {
    seq->prev = ctx->children.tail;
    ctx->children.tail->next = seq;
    ctx->children.tail = seq;
  }

  return 0;
}

int __rtld_lib_remove_dep(rtld_lib_t *ctx, rtld_lib_t *lib) {
  if (!ctx || !lib) {
    return -1;
  }

  for (rtld_lib_seq_t *it = ctx->children.head; it; it = it->next) {
    if (it->lib != lib) {
      continue;
    }

    if (it->prev) {
      it->prev->next = it->next;
    } else {
      ctx->children.head = it->next;
    }
    if (it->next) {
      it->next->prev = it->prev;
    } else {
      ctx->children.tail = it->prev;
    }

    free(it);

    return 0;
  }

  return -1;
}

int __rtld_lib_init(rtld_lib_t *ctx, int argc, char **argv, char **envp) {
  int err;

  if (!ctx) {
    return -1;
  }

  // init in reverse order
  for (rtld_lib_seq_t *it = ctx->children.tail; it; it = it->prev) {
    if ((err = __rtld_lib_init(it->lib, argc, argv, envp))) {
      return err;
    }
  }

  return ctx->init(ctx, argc, argv, envp);
}

rtld_lib_t *__rtld_lib_sym2lib(rtld_lib_t *ctx, const char *name) {
  rtld_lib_t *lib;

  if (!ctx || !name) {
    return 0;
  }

  if (ctx->sym2addr(ctx, name)) {
    return ctx;
  }

  // TODO: breadth-first search
  for (rtld_lib_seq_t *it = ctx->children.head; it; it = it->next) {
    if ((lib = __rtld_lib_sym2lib(it->lib, name))) {
      return lib;
    }
  }

  return 0;
}

rtld_lib_t *__rtld_lib_addr2lib(rtld_lib_t *ctx, void *addr) {
  rtld_lib_t *lib;

  if (!ctx || !addr) {
    return 0;
  }

  if (addr >= ctx->mapbase && addr < ctx->mapbase + ctx->mapsize) {
    return ctx;
  }

  for (rtld_lib_seq_t *it = ctx->children.head; it; it = it->next) {
    if ((lib = __rtld_lib_addr2lib(it->lib, addr))) {
      return lib;
    }
  }

  return 0;
}

void *__rtld_lib_sym2addr(rtld_lib_t *ctx, const char *name) {
  if (ctx && name) {
    return ctx->sym2addr(ctx, name);
  } else {
    return 0;
  }
}

const char *__rtld_lib_addr2sym(rtld_lib_t *ctx, void *addr) {
  if (ctx && addr) {
    return ctx->addr2sym(ctx, addr);
  } else {
    return 0;
  }
}

int __rtld_lib_fini(rtld_lib_t *ctx) {
  int err;

  if (!ctx) {
    return -1;
  }

  if ((err = ctx->fini(ctx))) {
    return err;
  }

  for (rtld_lib_seq_t *it = ctx->children.head; it; it = it->next) {
    if ((err = __rtld_lib_fini(it->lib))) {
      return err;
    }
  }

  return 0;
}

int __rtld_lib_close(rtld_lib_t *ctx) {
  int err;

  if (!ctx) {
    return -1;
  }

  ctx->refcnt--;
  if (ctx->refcnt > 0) {
    return 0;
  }

  if ((err = rtld_lib_close_deps(ctx, 0))) {
    return err;
  }

  if ((err = ctx->close(ctx))) {
    return err;
  }

  if (ctx->parent) {
    __rtld_lib_remove_dep(ctx->parent, ctx);
  }

  return 0;
}

void __rtld_lib_destroy(rtld_lib_t *ctx) {
  rtld_lib_seq_t *next;

  if (!ctx || ctx->refcnt > 0) {
    return;
  }

  if (ctx->parent) {
    __rtld_lib_remove_dep(ctx->parent, ctx);
  }

  for (rtld_lib_seq_t *it = ctx->children.head; it; it = next) {
    next = it->next;
    it->lib->parent = 0;
    free(it);
  }
  ctx->children.head = 0;
  ctx->children.tail = 0;

  ctx->destroy(ctx);
}

static int rtld_init_loaders(void) {
  int err;

  if ((err = __rtld_sprx_init())) {
    return err;
  }
  if ((err = __rtld_so_init())) {
    return err;
  }
  if ((err = __rtld_payload_init())) {
    return err;
  }
  if ((err = __rtld_dlfcn_init())) {
    return err;
  }

  return 0;
}

int __rtld_init(void) {
  int err;

  if (!KERNEL_DLSYM(0x2, strcpy)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, strcat)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, strcmp)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, strncmp)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, strlen)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, sprintf)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, calloc)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, free)) {
    return -1;
  }
  if (!KERNEL_DLSYM(0x2, getenv)) {
    return -1;
  }

  if (RTLD_JAIL_ENTER()) {
    return -1;
  }
  err = rtld_init_loaders();
  RTLD_JAIL_LEAVE();

  return err;
}
