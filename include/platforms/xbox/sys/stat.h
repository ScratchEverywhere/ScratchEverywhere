#pragma once
#ifndef _XBOX_SYS_STAT_H
#define _XBOX_SYS_STAT_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

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

static inline char *mkdtemp(char *tpl) {
    return NULL;
}

struct utimbuf {
    time_t actime;
    time_t modtime;
};

static inline int utime(const char *filename, const struct utimbuf *times) {
    return -1;
}

#ifdef MZ_OS_H
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

#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif
#ifndef S_IXUSR
#define S_IXUSR 00100
#endif
#ifndef S_IRGRP
#define S_IRGRP 00040
#endif
#ifndef S_IWGRP
#define S_IWGRP 00020
#endif
#ifndef S_IXGRP
#define S_IXGRP 00010
#endif
#ifndef S_IROTH
#define S_IROTH 00004
#endif
#ifndef S_IWOTH
#define S_IWOTH 00002
#endif
#ifndef S_IXOTH
#define S_IXOTH 00001
#endif
#ifndef S_IFMT
#define S_IFMT 0170000
#endif
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif

typedef unsigned int mode_t;
typedef unsigned int dev_t;
typedef unsigned int ino_t;
typedef unsigned int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

static inline int stat(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) return -1;
    char fixed_pathname[260];
    strncpy(fixed_pathname, pathname, sizeof(fixed_pathname) - 1);
    fixed_pathname[sizeof(fixed_pathname) - 1] = '\0';
    for (int i = 0; fixed_pathname[i]; i++) {
        if (fixed_pathname[i] == '/') fixed_pathname[i] = '\\';
    }
    DWORD attr = GetFileAttributesA(fixed_pathname);
    if (attr == INVALID_FILE_ATTRIBUTES) return -1;
    statbuf->st_mode = 0;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        statbuf->st_mode |= S_IFDIR;
    } else {
        statbuf->st_mode |= S_IFREG;
    }
    return 0;
}

static inline int lstat(const char *pathname, struct stat *statbuf) {
    return stat(pathname, statbuf);
}

static inline int mkdir(const char *pathname, mode_t mode) {
    return -1;
}

static inline int chmod(const char *pathname, mode_t mode) {
    return -1;
}

#endif