#pragma once
#ifndef _XBOX_UNISTD_H
#define _XBOX_UNISTD_H

#include <stdio.h>
#include <sys/types.h>

static inline int read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return -1;
}

static inline int write(int fd, const void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return -1;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return (off_t)-1;
}

static inline int close(int fd) {
    return -1;
}

static inline FILE *fdopen(int fd, const char *mode) {
    return NULL;
}

static inline int unlink(const char *pathname) {
    return -1;
}

static inline int rmdir(const char *pathname) {
    return -1;
}

static inline char *mkdtemp(char *template) {
    return NULL;
}

#endif
