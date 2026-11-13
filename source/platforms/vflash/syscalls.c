#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vflash/clock.h>
#include <vflash/fs.h>
#include <vflash/panic.h>
#include <wchar.h>

#define VFLASH_FD_OFFSET 3

extern char __heap_start;
extern char __heap_end;

void *_sbrk(ptrdiff_t incr) {
    (void)incr;
    // Memory allocation is handled by vflash-sdk's bounded alloc.c (malloc/free/realloc/calloc).
    // If newlib ever calls _sbrk directly, fail cleanly with ENOMEM.
    errno = ENOMEM;
    return (void *)-1;
}

int _open(const char *name, int flags, int mode) {
    (void)mode;
    if (!name) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & O_ACCMODE) != O_RDONLY || (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND))) {
        errno = EROFS;
        return -1;
    }
    int fd = vflash_fs_open(name);
    if (fd < 0) {
        errno = ENOENT;
        return -1;
    }
    return fd + VFLASH_FD_OFFSET;
}

int _close(int fd) {
    if (fd < VFLASH_FD_OFFSET) {
        return 0;
    }
    if (vflash_fs_close(fd - VFLASH_FD_OFFSET) != 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int _read(int fd, char *ptr, int len) {
    if (fd < VFLASH_FD_OFFSET) {
        return 0;
    }
    if (!ptr || len < 0) {
        errno = EINVAL;
        return -1;
    }
    int32_t n = vflash_fs_read(fd - VFLASH_FD_OFFSET, ptr, (uint32_t)len);
    if (n < 0) {
        errno = EIO;
        return -1;
    }
    return (int)n;
}

#define UART1_DATA_REG (*(volatile uint32_t *)0x90030000)
#define UART1_LSR_REG  (*(volatile uint32_t *)0x90030014)
#define UART_LSR_THRE  (1u << 5)

static void uart1_putc(char c) {
    uint32_t timeout = 10000u;
    while ((UART1_LSR_REG & UART_LSR_THRE) == 0 && --timeout) {
    }
    UART1_DATA_REG = (uint32_t)(uint8_t)c;
}

int _write(int fd, const char *ptr, int len) {
    if (fd == 1 || fd == 2) {
        for (int i = 0; i < len; ++i) {
            uart1_putc(ptr[i]);
        }
        return len;
    }
    errno = EROFS;
    return -1;
}

off_t _lseek(int fd, off_t ptr, int dir) {
    if (fd < VFLASH_FD_OFFSET) {
        errno = ESPIPE;
        return -1;
    }
    int32_t pos = vflash_fs_seek(fd - VFLASH_FD_OFFSET, (int32_t)ptr, dir);
    if (pos < 0) {
        errno = EINVAL;
        return -1;
    }
    return (off_t)pos;
}

int _fstat(int fd, struct stat *st) {
    if (!st) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < sizeof(*st); ++i) {
        ((uint8_t *)st)[i] = 0;
    }
    if (fd < VFLASH_FD_OFFSET) {
        st->st_mode = S_IFCHR;
        return 0;
    }
    int32_t sz = vflash_fs_size(fd - VFLASH_FD_OFFSET);
    if (sz < 0) {
        errno = EBADF;
        return -1;
    }
    st->st_mode = S_IFREG | 0444;
    st->st_size = (off_t)sz;
    return 0;
}

int _stat(const char *file, struct stat *st) {
    if (!file || !st) {
        errno = EINVAL;
        return -1;
    }
    int fd = vflash_fs_open(file);
    if (fd < 0) {
        errno = ENOENT;
        return -1;
    }
    int32_t sz = vflash_fs_size(fd);
    vflash_fs_close(fd);
    for (size_t i = 0; i < sizeof(*st); ++i) {
        ((uint8_t *)st)[i] = 0;
    }
    st->st_mode = S_IFREG | 0444;
    st->st_size = (sz >= 0) ? (off_t)sz : 0;
    return 0;
}

int _isatty(int fd) {
    return (fd >= 0 && fd < VFLASH_FD_OFFSET) ? 1 : 0;
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}

void abort(void) {
    vflash_panic(VFLASH_PANIC_FATAL, "abort() called");
    for (;;) {
    }
}

int mkdir(const char *path, mode_t mode) {
    (void)path;
    (void)mode;
    errno = EROFS;
    return -1;
}

int utime(const char *filename, const void *times) {
    (void)filename;
    (void)times;
    errno = EROFS;
    return -1;
}

int _unlink(const char *name) {
    (void)name;
    errno = EROFS;
    return -1;
}

int remove(const char *name) {
    return _unlink(name);
}

int _link(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    errno = EROFS;
    return -1;
}

int _getentropy(void *buf, size_t buflen) {
    uint8_t *p = (uint8_t *)buf;
    uint32_t seed = (uint32_t)vflash_clock_ticks() ^ 0xA5A55A5Au;
    for (size_t i = 0; i < buflen; ++i) {
        seed = seed * 1664525u + 1013904223u;
        p[i] = (uint8_t)(seed >> 24);
    }
    return 0;
}

// Single-threaded atomic builtins for ARMv5TEJ (no ldrex/strex)
unsigned int __atomic_fetch_add_4(volatile void *ptr, unsigned int val, int memorder) {
    (void)memorder;
    volatile unsigned int *p = (volatile unsigned int *)ptr;
    unsigned int old = *p;
    *p = old + val;
    return old;
}

unsigned int __atomic_fetch_sub_4(volatile void *ptr, unsigned int val, int memorder) {
    (void)memorder;
    volatile unsigned int *p = (volatile unsigned int *)ptr;
    unsigned int old = *p;
    *p = old - val;
    return old;
}

// Minimal wide-char stdio stubs for libstdc++_nano stdio_sync_filebuf<wchar_t>
#undef putwc
#undef getwc
wint_t putwc(wchar_t wc, FILE *stream) {
    (void)stream;
    return (wint_t)wc;
}

wint_t getwc(FILE *stream) {
    (void)stream;
    return WEOF;
}

wint_t ungetwc(wint_t wc, FILE *stream) {
    (void)stream;
    return wc;
}

int swprintf(wchar_t *ws, size_t len, const wchar_t *format, ...) {
    (void)format;
    if (ws && len > 0) {
        ws[0] = L'\0';
    }
    return 0;
}

FILE *freopen(const char *pathname, const char *mode, FILE *stream) {
    (void)pathname;
    (void)mode;
    (void)stream;
    errno = EROFS;
    return NULL;
}

int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        static uint16_t last_ticks = 0;
        static uint64_t total_ticks = 0;
        static int initialized = 0;
        uint16_t now = vflash_clock_ticks();
        if (!initialized) {
            last_ticks = now;
            initialized = 1;
        } else {
            total_ticks += (uint16_t)(now - last_ticks);
            last_ticks = now;
        }
        uint64_t us = total_ticks * 100ULL;
        tv->tv_sec = (time_t)(us / 1000000ULL);
        tv->tv_usec = (suseconds_t)(us % 1000000ULL);
    }
    return 0;
}

__attribute__((weak)) const unsigned int __exidx_start[1] = {0};
__attribute__((weak)) const unsigned int __exidx_end[1] = {0};
