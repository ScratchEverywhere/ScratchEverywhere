#ifndef _XBOX_FCNTL_H
#define _XBOX_FCNTL_H

#include <sys/stat.h>

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
#ifndef O_TRUNC
#define O_TRUNC 01000
#endif
#ifndef O_APPEND
#define O_APPEND 02000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static inline int open(const char *pathname, int flags, ...) {
    return -1;
}

#endif