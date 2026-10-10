#pragma once
#ifndef _XBOX_STDIO_H
#define _XBOX_STDIO_H

#include_next <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE *xbox_fopen(const char *filename, const char *mode);
#ifdef __cplusplus
}
#endif

#define fopen xbox_fopen

#ifdef __cplusplus
static inline int fopen_s(FILE **pFile, const char *filename, const char *mode) {
    *pFile = fopen(filename, mode);
    return *pFile ? 0 : 1;
}
static inline int _wfopen_s(FILE **pFile, const wchar_t *filename, const wchar_t *mode) {
    return 1;
}
static inline FILE *_wfopen(const wchar_t *filename, const wchar_t *mode) {
    return NULL;
}
#endif

#endif
