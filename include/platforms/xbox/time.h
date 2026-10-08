#ifndef _XBOX_TIME_H
#define _XBOX_TIME_H

#include_next <time.h>

static inline struct tm *localtime_r(const time_t *timep, struct tm *result) {
    struct tm *tm = localtime(timep);
    if (tm) {
        *result = *tm;
        return result;
    }
    return (struct tm *)0;
}

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp) {
    if (tp) {
        tp->tv_sec = 0;
        tp->tv_nsec = 0;
    }
    return -1;
}

#endif
