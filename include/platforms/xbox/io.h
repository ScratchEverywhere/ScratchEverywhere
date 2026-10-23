#pragma once
#ifndef _XBOX_IO_H
#define _XBOX_IO_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int read(int fd, void *buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

static inline int write(int fd, const void *buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return (off_t)-1;
}

static inline char *_mktemp(char *template_name) {
    (void)template_name;
    return NULL;
}

#ifndef _read
#define _read read
#endif
#ifndef _write
#define _write write
#endif
#ifndef _lseek
#define _lseek lseek
#endif
#ifndef _open
#define _open open
#endif
#ifndef _close
#define _close close
#endif

#ifndef mktemp
#define mktemp _mktemp
#endif

#ifdef __cplusplus
}
#endif

#endif
