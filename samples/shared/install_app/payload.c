/* Copyright (C) 2025 John Törnblom
   Copyright (C) 2026 ItsJokerZz

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/_iovec.h>
#include <sys/mount.h>
#include <sys/stat.h>

#ifdef __ORBIS__
#define SYSTEM_EX_DEV "/dev/da0x5.crypt"
#else
#define SYSTEM_EX_DEV "/dev/ssd0.system_ex"
#endif

#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))
#define IOVEC_ENTRY(x)                                                         \
  { x ? (void *)x : 0, x ? strlen(x) + 1 : 0 }

#define INCASSET(name, file)                                                   \
  __asm__(".section .rodata\n"                                                 \
          ".global " #name "\n"                                                \
          ".global " #name "_end\n"                                            \
          ".align 16\n" #name ":\n"                                            \
          ".incbin \"" file "\"\n" #name "_end:\n"                             \
          ".previous\n");                                                      \
  extern const uint8_t name[];                                                 \
  extern const uint8_t name##_end[];

int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppInstallTitleDir(const char *, const char *, void *);

INCASSET(eboot, EBOOT);
INCASSET(icon0, ICON0);
INCASSET(user_param, USER_PARAM);
INCASSET(system_param, SYSTEM_PARAM);

static int remount_system_ex(void) {
  struct iovec iov[] = {
      IOVEC_ENTRY("from"),      IOVEC_ENTRY(SYSTEM_EX_DEV),
      IOVEC_ENTRY("fspath"),    IOVEC_ENTRY("/system_ex"),
      IOVEC_ENTRY("fstype"),    IOVEC_ENTRY("exfatfs"),
      IOVEC_ENTRY("large"),     IOVEC_ENTRY("yes"),
      IOVEC_ENTRY("timezone"),  IOVEC_ENTRY("static"),
      IOVEC_ENTRY("async"),     IOVEC_ENTRY(NULL),
      IOVEC_ENTRY("ignoreacl"), IOVEC_ENTRY(NULL),
  };

  return nmount(iov, IOVEC_SIZE(iov), MNT_UPDATE);
}

static int make_dir(const char *path) {
  if (mkdir(path, 0777) && errno != EEXIST) {
    perror(path);
    return -1;
  }

  return 0;
}

static int put_file(const char *path, const uint8_t *data, const uint8_t *end) {
  size_t size = end - data;
  int fd;

  if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0) {
    perror(path);
    return -1;
  }

  if (write(fd, data, size) != (ssize_t)size) {
    perror(path);
    close(fd);
    return -1;
  }

  fchmod(fd, 0777);
  close(fd);

  return 0;
}

int main(int argc, char *argv[]) {
  int err;

  umask(0);

  if (remount_system_ex()) {
    perror("remount /system_ex");
    return -1;
  }

  if (make_dir("/system_ex/app/" TITLE_ID) ||
      make_dir("/system_ex/app/" TITLE_ID "/sce_sys") ||
      make_dir("/user/app/" TITLE_ID) ||
      make_dir("/user/app/" TITLE_ID "/sce_sys")) {
    return -1;
  }

  if (put_file("/system_ex/app/" TITLE_ID "/eboot.bin", eboot, eboot_end) ||
      put_file("/user/app/" TITLE_ID "/sce_sys/" PARAM, user_param,
               user_param_end) ||
      put_file("/user/app/" TITLE_ID "/sce_sys/icon0.png", icon0, icon0_end)) {
    return -1;
  }

  if ((err = sceAppInstUtilInitialize())) {
    printf("sceAppInstUtilInitialize: %x\n", err);
    return -1;
  }

  if ((err = sceAppInstUtilAppInstallTitleDir(TITLE_ID, "/user/app/", 0))) {
    printf("sceAppInstUtilAppInstallTitleDir: %x\n", err);
    return -1;
  }

  if (put_file("/system_ex/app/" TITLE_ID "/sce_sys/" PARAM, system_param,
               system_param_end)) {
    return -1;
  }

  return 0;
}
