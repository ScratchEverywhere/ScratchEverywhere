#pragma once
#ifndef _XBOX_DIRECT_H
#define _XBOX_DIRECT_H

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int _mkdir(const char *dirname) {
    // POSIX mkdir takes two arguments, Windows _mkdir takes one.
    return mkdir(dirname, 0777);
}

#ifdef __cplusplus
}
#endif

#endif
