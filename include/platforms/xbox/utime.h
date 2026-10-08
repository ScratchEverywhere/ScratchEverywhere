#ifndef _XBOX_UTIME_H
#define _XBOX_UTIME_H

#include <time.h>

struct utimbuf {
    time_t actime;
    time_t modtime;
};

static inline int utime(const char *filename, const struct utimbuf *times) {
    return -1;
}

#endif
