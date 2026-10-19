#pragma once
#ifndef _XBOX_FCNTL_H
#define _XBOX_FCNTL_H

#include <sys/stat.h>
#include <stdarg.h>

#ifndef O_RDONLY
#define O_RDONLY 00
#endif
#ifndef O_WRONLY
#define O_WRONLY 01
#endif
#ifndef O_RDWR
#define O_RDWR 02
#endif
#ifndef O_CREAT
#define O_CREAT 0100
#endif
#ifndef O_EXCL
#define O_EXCL 0200
#endif
#ifndef O_TRUNC
#define O_TRUNC 01000
#endif
#ifndef O_APPEND
#define O_APPEND 02000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
int lwip_fcntl(int s, int cmd, int val);
#ifdef __cplusplus
}
#endif

#ifdef fcntl
#undef fcntl
#endif

static inline int fcntl(int s, int cmd, ...) {
    int val = 0;
    va_list va;
    va_start(va, cmd);
    val = va_arg(va, int);
    va_end(va);
    return lwip_fcntl(s, cmd, val);
}

static inline int open(const char *pathname, int flags, ...) {
    (void)pathname;
    (void)flags;
    return -1;
}

#endif /* _XBOX_FCNTL_H */