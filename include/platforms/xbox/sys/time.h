#pragma once
#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <time.h>

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
#define LWIP_TIMEVAL_PRIVATE 0
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

#ifndef _GETTIMEOFDAY_DEFINED
#define _GETTIMEOFDAY_DEFINED
static inline int gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        tv->tv_sec = (long)time(NULL);
        tv->tv_usec = 0;
    }
    (void)tz;
    return 0;
}
#endif

#endif /* _SYS_TIME_H */
