#pragma once
#ifndef _XBOX_DIRENT_H
#define _XBOX_DIRENT_H

struct dirent {
    unsigned long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

typedef struct {
    int dummy;
} DIR;

static inline DIR *opendir(const char *name) {
    return (DIR *)0;
}

static inline struct dirent *readdir(DIR *dirp) {
    return (struct dirent *)0;
}

static inline int closedir(DIR *dirp) {
    return -1;
}

#endif
