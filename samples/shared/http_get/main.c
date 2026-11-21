/* Copyright (C) 2024 John Törnblom

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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__PROSPERO__)
#include <ps5/klog.h>
#else
#include <ps4/klog.h>
#endif

// log to stdout and to the klog, the payload output does not always reach it
#define LOG(...)                                                               \
  do {                                                                         \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
    klog_printf(__VA_ARGS__);                                                  \
    klog_printf("\n");                                                         \
  } while (0)

// the sce libraries return a negative error code, not an errno
#define STEP(var, call)                                                        \
  do {                                                                         \
    (var) = (call);                                                            \
    if ((var) < 0) {                                                           \
      LOG("[-] %s failed: 0x%08x", #call, (unsigned)(var));                    \
      return -1;                                                               \
    }                                                                          \
    LOG("[+] %s = %d", #call, (int)(var));                                     \
  } while (0)

#define CLEANUP(call)                                                          \
  do {                                                                         \
    int r_ = (call);                                                           \
    if (r_) {                                                                  \
      LOG("[-] %s failed: 0x%08x", #call, (unsigned)r_);                       \
    }                                                                          \
  } while (0)

int sceNetInit();
int sceNetPoolCreate(const char *, int, int);
int sceNetPoolDestroy(int);

int sceNetCtlInit(void);
void sceNetCtlTerm(void);

int sceSslInit(size_t);
int sceSslTerm(int);

int sceHttpInit(int, int, size_t);
int sceHttpTerm(int);

int sceHttpCreateTemplate(int, const char *, int, int);
int sceHttpDeleteTemplate(int);

int sceHttpCreateConnectionWithURL(int, const char *, int);
int sceHttpDeleteConnection(int);

int sceHttpCreateRequestWithURL(int, int, const char *, uint64_t);
int sceHttpDeleteRequest(int);

int sceHttpSendRequest(int, const void *, size_t);
int sceHttpGetStatusCode(int, int *);
int sceHttpReadData(int, void *, size_t);

#define SCE_HTTP_VERSION_1_1 2
#define SCE_HTTP_METHOD_GET 0

static int g_libnetMemId = -1;
static int g_libsslCtxId = -1;
static int g_libhttpCtxId = -1;
static int g_tmplId = -1;
static int g_connId = -1;
static int g_reqId = -1;

static int http_init(const char *agent, const char *url) {
  int r;

  STEP(r, sceNetInit());
  // sceNetCtlInit() does not return when called from this payload, and the
  // console already has it initialized, so it is left out
  STEP(g_libnetMemId, sceNetPoolCreate("http_get", 64 * 1024, 0));
  STEP(g_libsslCtxId, sceSslInit(512 * 1024));
  STEP(g_libhttpCtxId, sceHttpInit(g_libnetMemId, g_libsslCtxId, 256 * 1024));
  STEP(g_tmplId,
       sceHttpCreateTemplate(g_libhttpCtxId, agent, SCE_HTTP_VERSION_1_1, 1));
  STEP(g_connId, sceHttpCreateConnectionWithURL(g_tmplId, url, 0));
  STEP(g_reqId, sceHttpCreateRequestWithURL(g_connId, SCE_HTTP_METHOD_GET, url,
                                            0));
  return 0;
}

static int http_get(void) {
  char buf[0x1000];
  int length = -1;
  int status = -1;
  int total = 0;
  int r;

  STEP(r, sceHttpSendRequest(g_reqId, NULL, 0));
  STEP(r, sceHttpGetStatusCode(g_reqId, &status));
  LOG("[+] HTTP status: %d", status);

  if (status == 200) {
    while ((length = sceHttpReadData(g_reqId, buf, sizeof(buf) - 1)) > 0) {
      buf[length] = 0;
      total += length;
      if (total == length) {
        // first chunk only, enough to see that the body is right
        LOG("[+] body starts: %.80s", buf);
      }
    }
    if (length < 0) {
      LOG("[-] sceHttpReadData failed: 0x%08x", (unsigned)length);
      return -1;
    }
    LOG("[+] read %d bytes", total);
  }

  return status;
}

static void http_fini(void) {
  if (g_reqId >= 0)
    CLEANUP(sceHttpDeleteRequest(g_reqId));
  if (g_connId >= 0)
    CLEANUP(sceHttpDeleteConnection(g_connId));
  if (g_tmplId >= 0)
    CLEANUP(sceHttpDeleteTemplate(g_tmplId));
  if (g_libhttpCtxId >= 0)
    CLEANUP(sceHttpTerm(g_libhttpCtxId));
  if (g_libsslCtxId >= 0)
    CLEANUP(sceSslTerm(g_libsslCtxId));
  if (g_libnetMemId >= 0)
    CLEANUP(sceNetPoolDestroy(g_libnetMemId));
}

int main() {
  int error = 0;

  LOG("[*] http_get starting");

  if (!(error = http_init("http_get/1.0", "http://example.com"))) {
    error = http_get();
  }

  http_fini();

  LOG("[*] http_get done, result %d", error);

  return error;
}
