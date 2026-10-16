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

#ifndef mktemp
#define mktemp _mktemp
#endif

#ifdef __cplusplus
}
#endif

#endif
