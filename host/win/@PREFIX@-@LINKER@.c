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

#include <libgen.h>
#include <limits.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Quote an argument for the command line of the child, so that the runtime of
 * the child splits it the same way again.
 **/
static char *quote(const char *arg) {
  size_t len = strlen(arg);
  char *out;
  char *p;

  if (len && !strpbrk(arg, " \t\n\v\"")) {
    return strdup(arg);
  }

  if (!(out = malloc(len * 2 + 3))) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  p = out;
  *p++ = '"';
  for (size_t i = 0;; i++) {
    size_t slashes = 0;

    while (arg[i] == '\\') {
      slashes++;
      i++;
    }

    if (arg[i] == '\0') {
      memset(p, '\\', slashes * 2);
      p += slashes * 2;
      break;
    } else if (arg[i] == '"') {
      memset(p, '\\', slashes * 2 + 1);
      p += slashes * 2 + 1;
      *p++ = '"';
    } else {
      memset(p, '\\', slashes);
      p += slashes;
      *p++ = arg[i];
    }
  }
  *p++ = '"';
  *p = '\0';

  return out;
}

int main(int argc, char **argv) {
  char sdkdir[PATH_MAX];
  char ldscript[PATH_MAX * 2];
  const char *str;
  char **args;
  intptr_t ret;
  int script = 1;
  int pie = 1;
  int n = 0;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shared") || !strcmp(argv[i], "-r") ||
        !strcmp(argv[i], "--static")) {
      pie = 0;
    }
    if (!strcmp(argv[i], "--script") || !strcmp(argv[i], "-T") ||
        !strncmp(argv[i], "--script=", 9)) {
      script = 0;
    }
  }

  if ((str = getenv("@PLATFORM_UC@_PAYLOAD_SDK"))) {
    snprintf(sdkdir, sizeof(sdkdir), "%s", str);
  } else if ((str = dirname(argv[0])) &&
             _fullpath(sdkdir, str, sizeof(sdkdir) - 4)) {
    strcat(sdkdir, "/..");
  } else {
    fprintf(stderr, "@PLATFORM_UC@_PAYLOAD_SDK is not set\n");
    return EXIT_FAILURE;
  }

  if ((size_t)snprintf(ldscript, sizeof(ldscript), "%s/ldscripts/elf_x86_64.x",
                       sdkdir) >= sizeof(ldscript)) {
    fprintf(stderr, "@PLATFORM_UC@_PAYLOAD_SDK is too long\n");
    return EXIT_FAILURE;
  }

  if (!(args = calloc(argc + 16, sizeof(char *)))) {
    perror("calloc");
    return EXIT_FAILURE;
  }

  args[n++] = quote("ld.lld");
  args[n++] = quote("-m");
  args[n++] = quote("elf_x86_64");
  if (pie) {
    args[n++] = quote("-pie");
  }
  if (script) {
    args[n++] = quote("-T");
    args[n++] = quote(ldscript);
  }
  args[n++] = quote("--eh-frame-hdr");
  args[n++] = quote("-z");
  args[n++] = quote("max-page-size=0x4000");
  args[n++] = quote("-mllvm");
  args[n++] = quote("-emulated-tls");
  for (int i = 1; i < argc; i++) {
    args[n++] = quote(argv[i]);
  }
  args[n++] = quote("--hash-style=gnu");

  if ((ret = _spawnvp(_P_WAIT, "ld.lld.exe", (const char *const *)args)) ==
      -1) {
    perror("ld.lld");
    return EXIT_FAILURE;
  }

  return (int)ret;
}
