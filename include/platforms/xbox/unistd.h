#pragma once
#ifndef _XBOX_UNISTD_H
#define _XBOX_UNISTD_H

#include <stdio.h>

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
