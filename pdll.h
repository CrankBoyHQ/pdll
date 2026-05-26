/*
 * pdll.h - Playdate Dynamic Library Linker
 *
 */
#ifndef PDLL_H
#define PDLL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pd_api.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// magic value for event hander init to know it's a dynamic library
#define PDLL_DYNAMIC_INIT_ARG 0x17403900u

typedef void *(*pdll_getsymbol_t)(const char *symbol);

typedef int (*pdll_eventhandler_t)(PlaydateAPI *, PDSystemEvent, uint32_t);

// pdll_open flags
#define PDLL_FILE_PDX (1u << 0)
#define PDLL_FILE_DATA (1u << 1) /* per-game data   (kFileReadData) */
#define PDLL_NO_INIT (1u << 2)  /* don't run eventHandler's init */
#define PDLL_NO_TERM (1u << 3) /* don't run eventHandler's close */
#define PDLL_ALIGN_32 (1u << 4)
#define PDLL_ALIGN_256 (1u << 5)
#define PDLL_ALIGN_4K (1u << 6)

typedef struct pdll_s {
  struct {
    const struct playdate_sys *system;
    const struct playdate_file *file;
    const struct playdate_graphics *graphics;
  } _playdate_slice;

  // pointer to the real API (same contents as the copy above)
  PlaydateAPI *playdate_ptr;

  // library should set this
  pdll_getsymbol_t getSymbol;

  pdll_eventhandler_t eventHandler;

  const char *path;

  uint32_t flags;
  void *image;     // aligned
  void *image_raw; // original alloc
#ifdef TARGET_SIMULATOR
  void *dl;
  char tmppath[1024];
#endif
} pdll_t;

typedef struct {
  const char *name;
  void *addr;
} pdll_export_t;

/* map macro */
#define PDLL__EVAL0(...) __VA_ARGS__
#define PDLL__EVAL1(...) PDLL__EVAL0(PDLL__EVAL0(PDLL__EVAL0(__VA_ARGS__)))
#define PDLL__EVAL2(...) PDLL__EVAL1(PDLL__EVAL1(PDLL__EVAL1(__VA_ARGS__)))
#define PDLL__EVAL(...) PDLL__EVAL2(PDLL__EVAL2(PDLL__EVAL2(__VA_ARGS__)))
#define PDLL__MAP_END(...)
#define PDLL__MAP_OUT
#define PDLL__MAP_GET_END2() 0, PDLL__MAP_END
#define PDLL__MAP_GET_END1(...) PDLL__MAP_GET_END2
#define PDLL__MAP_GET_END(...) PDLL__MAP_GET_END1
#define PDLL__MAP_NEXT0(test, next, ...) next PDLL__MAP_OUT
#define PDLL__MAP_NEXT1(test, next) PDLL__MAP_NEXT0(test, next, 0)
#define PDLL__MAP_NEXT(test, next) PDLL__MAP_NEXT1(PDLL__MAP_GET_END test, next)
#define PDLL__MAP0(f, x, peek, ...)                                            \
  f(x) PDLL__MAP_NEXT(peek, PDLL__MAP1)(f, peek, __VA_ARGS__)
#define PDLL__MAP1(f, x, peek, ...)                                            \
  f(x) PDLL__MAP_NEXT(peek, PDLL__MAP0)(f, peek, __VA_ARGS__)
#define PDLL__MAP(f, ...)                                                      \
  PDLL__EVAL(PDLL__MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/* empty-__VA_ARGS__ detection */
#define PDLL__ARG3(a, b, c, ...) c
#define PDLL__HAS_COMMA(...) PDLL__ARG3(__VA_ARGS__, 1, 0)
#define PDLL__TRIGGER(...) ,
#define PDLL__PASTE5(a, b, c, d, e) a##b##c##d##e
#define PDLL__IS_EMPTY_CASE_0001 ,
#define PDLL__ISEMPTY_(a, b, c, d)                                             \
  PDLL__HAS_COMMA(PDLL__PASTE5(PDLL__IS_EMPTY_CASE_, a, b, c, d))
#define PDLL__ISEMPTY(...)                                                     \
  PDLL__ISEMPTY_(PDLL__HAS_COMMA(__VA_ARGS__),                                 \
                 PDLL__HAS_COMMA(PDLL__TRIGGER __VA_ARGS__),                   \
                 PDLL__HAS_COMMA(__VA_ARGS__()),                               \
                 PDLL__HAS_COMMA(PDLL__TRIGGER __VA_ARGS__()))

#define PDLL__STR(x) #x
#define PDLL__EXPORT_ENTRY(fn) {PDLL__STR(fn), (void *)(fn)},
#define PDLL__EXPORTS_1(...)
#define PDLL__EXPORTS_0(...) PDLL__MAP(PDLL__EXPORT_ENTRY, __VA_ARGS__)
#define PDLL__CAT(a, b) a##b
#define PDLL__XCAT(a, b) PDLL__CAT(a, b)

#define PDLL_EXPORT(...)                                                       \
  static const pdll_export_t pdll__exports[] = {                               \
      PDLL__XCAT(PDLL__EXPORTS_, PDLL__ISEMPTY(__VA_ARGS__))(__VA_ARGS__){     \
          (const char *)0, (void *)0}};                                        \
  static void *pdll_getsymbol_impl(const char *symbol) {                       \
    const pdll_export_t *e;                                                    \
    for (e = pdll__exports; e->name; ++e)                                      \
      if (!strcmp(symbol, e->name))                                            \
        return e->addr;                                                        \
    return NULL;                                                               \
  }

#define PDLL_EVENT(playdate, event, arg)                                       \
  pdll_t *pdll = ((event) == kEventInit && (arg) == PDLL_DYNAMIC_INIT_ARG)     \
                     ? (pdll_t *)(void *)(playdate)                            \
                     : (pdll_t *)0;                                            \
  if (pdll) {                                                                  \
    pdll->getSymbol = pdll_getsymbol_impl;                                     \
    (playdate) = pdll->playdate_ptr;                                              \
  }

const char *pdll_get_error(void);
pdll_t *pdll_open(PlaydateAPI *pd, const char *path, uint32_t flags);
void pdll_close(pdll_t *lib);
void *pdll_symbol(pdll_t *lib, const char *symbol);

#ifdef PDLL_IMPLEMENTATION

#include <stdarg.h>
#include <string.h>

#ifdef TARGET_SIMULATOR
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#else
#include "uzlib/uzlib.h" /* pdex.bin format needs decompression */
#endif

#define PDLL_ERR_BUF 256
static char pdll__err[PDLL_ERR_BUF] = "";
static PlaydateAPI *pdll__pd = NULL;

const char *pdll_get_error(void) { return pdll__err; }

static void pdll__seterr(const char *fmt, ...) {
  if (!pdll__pd) {
    pdll__err[0] = 0;
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  char *msg = NULL;
  pdll__pd->system->vaFormatString(&msg, fmt, ap);
  va_end(ap);
  if (!msg)
    return;
  size_t n = strlen(msg);
  if (n >= sizeof(pdll__err))
    n = sizeof(pdll__err) - 1;
  memcpy(pdll__err, msg, n);
  pdll__err[n] = 0;
  pdll__pd->system->realloc(msg, 0);
}

static uint32_t pdll__alignment(uint32_t flags) {
  if (flags & PDLL_ALIGN_4K)
    return 4096;
  if (flags & PDLL_ALIGN_256)
    return 256;
  if (flags & PDLL_ALIGN_32)
    return 32;
  return 2;
}

static int pdll__fsflags(uint32_t flags, FileOptions *out) {
  int f = 0;
  if (flags & PDLL_FILE_PDX)
    f |= kFileRead;
  if (flags & PDLL_FILE_DATA)
    f |= kFileReadData;
  if (!f)
    return 0;
  *out = (FileOptions)f;
  return 1;
}

/* ----- pdex.bin container (produced by pdc) ----- */
typedef struct {
  char sig[12]; /* "Playdate ..." */
  uint32_t flags;
  uint8_t md5[16];
  uint32_t p_filesz;
  uint32_t p_memsz; /* total image size including bss*/
  uint32_t e_entry;
  uint32_t nreloc;
} pdll__hdr_t;

#define PDLL_PDEX_ENCRYPTED 0x40000000u

static int pdll__check_magic(const pdll__hdr_t *h) {
  static const char m[8] = "PLAYDATE";
  for (int i = 0; i < 8; ++i) {
    char c = h->sig[i];
    if (c >= 'a' && c <= 'z')
      c -= ('a' - 'A');
    if (c != m[i])
      return 0;
  }
  return 1;
}

#ifndef TARGET_SIMULATOR

#define PDLL_UZ_INBUF 1024
static SDFile *pdll__uz_file;
static PlaydateAPI *pdll__uz_pd;
static unsigned char pdll__uz_inbuf[PDLL_UZ_INBUF];

static int pdll__uz_read(struct uzlib_uncomp *d) {
  int n = pdll__uz_pd->file->read(pdll__uz_file, pdll__uz_inbuf, PDLL_UZ_INBUF);
  if (n <= 0)
    return -1;
  d->source = pdll__uz_inbuf + 1;
  d->source_limit = pdll__uz_inbuf + n;
  return pdll__uz_inbuf[0];
}

pdll_t *pdll_open(PlaydateAPI *pd, const char *path, uint32_t flags) {
  pdll__pd = pd;
  FileOptions fo;
  if (!pdll__fsflags(flags, &fo)) {
    pdll__seterr(
        "pdll_open: no filesystem flag (need PDLL_FILE_PDX or PDLL_FILE_DATA)");
    return NULL;
  }

  char name[256];
  size_t pl = strlen(path);
  if (pl + 5 > sizeof(name)) /* + ".bin\0" */
  {
    pdll__seterr("pdll_open: path too long");
    return NULL;
  }
  memcpy(name, path, pl);
  memcpy(name + pl, ".bin", 5);

  SDFile *file = pd->file->open(name, fo);
  if (!file) {
    pdll__seterr("pdll_open: cannot open '%s'", name);
    return NULL;
  }

  pdll__hdr_t hdr;
  if (pd->file->read(file, &hdr, sizeof(hdr)) != (int)sizeof(hdr)) {
    pdll__seterr("pdll_open: short header read on '%s'", name);
    pd->file->close(file);
    return NULL;
  }
  if (!pdll__check_magic(&hdr)) {
    pdll__seterr("pdll_open: '%s' is not a pdex.bin (bad magic)", name);
    pd->file->close(file);
    return NULL;
  }
  if (hdr.flags & PDLL_PDEX_ENCRYPTED) {
    pdll__seterr("pdll_open: encrypted pdex.bin not supported");
    pd->file->close(file);
    return NULL;
  }

  size_t total = (size_t)hdr.p_filesz + (size_t)hdr.nreloc * 4u;
  char *dec = (char *)pd->system->realloc(NULL, total);
  if (!dec) {
    pdll__seterr("pdll_open: out of memory (%u-byte decode buffer)",
                 (unsigned)total);
    pd->file->close(file);
    return NULL;
  }

  uzlib_init();
  struct uzlib_uncomp d;
  memset(&d, 0, sizeof(d));
  pdll__uz_file = file;
  pdll__uz_pd = pd;
  d.source_read_cb = pdll__uz_read;
  d.dest = d.dest_start = (unsigned char *)dec;
  d.dest_limit = (unsigned char *)dec + total;
  uzlib_uncompress_init(&d, NULL, 0);

  if (uzlib_zlib_parse_header(&d) < 0) {
    pdll__seterr("pdll_open: bad zlib header in '%s'", name);
    pd->system->realloc(dec, 0);
    pd->file->close(file);
    return NULL;
  }
  int ir = uzlib_uncompress(&d);
  pd->file->close(file);
  if (ir < 0 || (size_t)(d.dest - d.dest_start) != total) {
    pdll__seterr("pdll_open: inflate failed (%d, %u/%u bytes)", ir,
                 (unsigned)(d.dest - d.dest_start), (unsigned)total);
    pd->system->realloc(dec, 0);
    return NULL;
  }

  /* allocate the text region */
  uint32_t align = pdll__alignment(flags);
  size_t slack = (align > 8) ? (align - 1) : 0;
  void *raw = pd->system->realloc(NULL, (size_t)hdr.p_memsz + slack);
  if (!raw) {
    pdll__seterr("pdll_open: out of memory (%u-byte image)",
                 (unsigned)hdr.p_memsz);
    pd->system->realloc(dec, 0);
    return NULL;
  }
  uintptr_t base = ((uintptr_t)raw + (align - 1)) & ~(uintptr_t)(align - 1);

  /* copy text + zero bss */
  memcpy((void *)base, dec, hdr.p_filesz);
  memset((void *)(base + hdr.p_filesz), 0, hdr.p_memsz - hdr.p_filesz);

  /* apply relocations */
  const uint32_t *relocs = (const uint32_t *)(dec + hdr.p_filesz);
  for (uint32_t i = 0; i < hdr.nreloc; ++i)
    *(uint32_t *)(base + relocs[i]) += (uint32_t)base;

  pd->system->realloc(dec, 0);

  pd->system->clearICache();
  __asm__ volatile("dsb\n\tisb" ::: "memory");

  pdll_t *lib = (pdll_t *)pd->system->realloc(NULL, sizeof(pdll_t));
  if (!lib) {
    pdll__seterr("pdll_open: out of memory (handle)");
    pd->system->realloc(raw, 0);
    return NULL;
  }
  memset(lib, 0, sizeof(*lib));
  // copy the leading fields at offset 0 so the handle looks like a PlaydateAPI
  lib->_playdate_slice.system = pd->system;
  lib->_playdate_slice.file = pd->file;
  lib->_playdate_slice.graphics = pd->graphics;
  lib->playdate_ptr = pd;
  lib->path = path;
  lib->flags = flags;
  lib->image = (void *)base;
  lib->image_raw = raw;
  lib->eventHandler = (pdll_eventhandler_t)(base + hdr.e_entry);

  if (!(flags & PDLL_NO_INIT))
    lib->eventHandler((PlaydateAPI *)lib, kEventInit, PDLL_DYNAMIC_INIT_ARG);

  return lib;
}

void pdll_close(pdll_t *lib) {
  if (!lib)
    return;
  PlaydateAPI *pd = lib->playdate_ptr;
  if (!(lib->flags & PDLL_NO_TERM))
    lib->eventHandler(pd, kEventTerminate, 0);
  pd->system->realloc(lib->image_raw, 0);
  pd->system->realloc(lib, 0);
}

void *pdll_symbol(pdll_t *lib, const char *symbol) {
  if (!lib || !lib->getSymbol) {
    pdll__seterr("pdll_symbol: library registered no resolver");
    return NULL;
  }
  void *r = lib->getSymbol(symbol);
  if (!r)
    pdll__seterr("pdll_symbol: '%s' unresolved", symbol ? symbol : "(null)");
  return r;
}

#else /* TARGET_SIMULATOR */

#if defined(__APPLE__)
#define PDLL_DLEXT ".dylib"
#elif defined(_WIN32)
#define PDLL_DLEXT ".dll"
#else
#define PDLL_DLEXT ".so"
#endif

static void *pdll__read_all(PlaydateAPI *pd, const char *name, FileOptions fo,
                            int *out_len) {
  SDFile *f = pd->file->open(name, fo);
  if (!f)
    return NULL;
  pd->file->seek(f, 0, SEEK_END);
  int sz = pd->file->tell(f);
  pd->file->seek(f, 0, SEEK_SET);
  if (sz <= 0) {
    pd->file->close(f);
    return NULL;
  }
  char *buf = (char *)malloc((size_t)sz);
  if (!buf) {
    pd->file->close(f);
    return NULL;
  }
  int got = pd->file->read(f, buf, sz);
  pd->file->close(f);
  if (got != sz) {
    free(buf);
    return NULL;
  }
  *out_len = sz;
  return buf;
}

/* Write bytes to a fresh temp file; store the path in tmppath. */
static int pdll__write_temp(const void *data, int len, char *tmppath,
                            size_t cap) {
#ifdef _WIN32
  char dir[MAX_PATH];
  if (!GetTempPathA(sizeof(dir), dir))
    return 0;
  if (!GetTempFileNameA(dir, "pdll", 0, tmppath))
    return 0;
  FILE *fp = fopen(tmppath, "wb");
  if (!fp)
    return 0;
  int ok = ((int)fwrite(data, 1, len, fp) == len);
  fclose(fp);
  return ok;
#else
  snprintf(tmppath, cap, "/tmp/pdll-XXXXXX%s", PDLL_DLEXT);
  int fd = mkstemps(tmppath, (int)strlen(PDLL_DLEXT));
  if (fd < 0)
    return 0;
  int ok = (write(fd, data, (size_t)len) == (ssize_t)len);
  close(fd);
  return ok;
#endif
}

pdll_t *pdll_open(PlaydateAPI *pd, const char *path, uint32_t flags) {
  pdll__pd = pd;
  FileOptions fo;
  if (!pdll__fsflags(flags, &fo)) {
    pdll__seterr(
        "pdll_open: no filesystem flag (need PDLL_FILE_PDX or PDLL_FILE_DATA)");
    return NULL;
  }

  char name[256];
  if (snprintf(name, sizeof(name), "%s%s", path, PDLL_DLEXT) >=
      (int)sizeof(name)) {
    pdll__seterr("pdll_open: path too long");
    return NULL;
  }

  int len = 0;
  void *bytes = pdll__read_all(pd, name, fo, &len);
  if (!bytes) {
    pdll__seterr("pdll_open: cannot read '%s' through the simulator VFS", name);
    return NULL;
  }

  pdll_t *lib = (pdll_t *)calloc(1, sizeof(pdll_t));
  if (!lib) {
    free(bytes);
    pdll__seterr("pdll_open: out of memory");
    return NULL;
  }

  if (!pdll__write_temp(bytes, len, lib->tmppath, sizeof(lib->tmppath))) {
    free(bytes);
    free(lib);
    pdll__seterr("pdll_open: failed to stage temp copy of '%s'", name);
    return NULL;
  }
  free(bytes);

#ifdef _WIN32
  HMODULE h = LoadLibraryA(lib->tmppath);
  if (!h) {
    DeleteFileA(lib->tmppath);
    free(lib);
    pdll__seterr("pdll_open: LoadLibrary failed for '%s'", name);
    return NULL;
  }
  lib->dl = (void *)h;
  lib->eventHandler = (pdll_eventhandler_t)GetProcAddress(h, "eventHandlerShim");
  if (!lib->eventHandler)
    lib->eventHandler = (pdll_eventhandler_t)GetProcAddress(h, "eventHandler");
#endif

#ifndef _WIN32
  void *h = dlopen(lib->tmppath, RTLD_NOW | RTLD_LOCAL);
  /* mapping persists after unlink on POSIX */
  unlink(lib->tmppath);
  lib->tmppath[0] = 0;
  if (!h) {
    free(lib);
    pdll__seterr("pdll_open: dlopen failed: %s", dlerror());
    return NULL;
  }
  lib->dl = h;
  // Prefer the SDK shim entrypoint (matches device, where e_entry is the shim):
  // on kEventInit it sets the library's libc allocator hook from the API copy
  // we pass, so malloc works inside the library.
  lib->eventHandler = (pdll_eventhandler_t)dlsym(h, "eventHandlerShim");
  if (!lib->eventHandler)
    lib->eventHandler = (pdll_eventhandler_t)dlsym(h, "eventHandler");
#endif

  if (!lib->eventHandler) {
    pdll__seterr("pdll_open: '%s' exports no eventHandler", name);
    pdll_close(lib);
    return NULL;
  }

  // copy the leading fields at offset 0 so the handle looks like a PlaydateAPI
  lib->playdate.system = pd->system;
  lib->playdate.file = pd->file;
  lib->playdate.graphics = pd->graphics;
  lib->playdate_ptr = pd;
  lib->path = path;
  lib->flags = flags;
  if (!(flags & PDLL_NO_INIT))
    lib->eventHandler((PlaydateAPI *)lib, kEventInit, PDLL_DYNAMIC_INIT_ARG);
  return lib;
}

void pdll_close(pdll_t *lib) {
  if (!lib)
    return;
  if (lib->eventHandler && lib->playdate_ptr && !(lib->flags & PDLL_NO_TERM))
    lib->eventHandler(lib->playdate_ptr, kEventTerminate, 0);
#ifdef _WIN32
  if (lib->dl)
    FreeLibrary((HMODULE)lib->dl);
  if (lib->tmppath[0])
    DeleteFileA(lib->tmppath);
#else
  if (lib->dl)
    dlclose(lib->dl);
#endif
  free(lib);
}

void *pdll_symbol(pdll_t *lib, const char *symbol) {
  if (!lib || !lib->getSymbol) {
    pdll__seterr("pdll_symbol: library registered no resolver");
    return NULL;
  }
  void *r = lib->getSymbol(symbol);
  if (!r)
    pdll__seterr("pdll_symbol: '%s' unresolved", symbol ? symbol : "(null)");
  return r;
}

#endif /* TARGET_SIMULATOR */

#endif /* PDLL_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* PDLL_H */
