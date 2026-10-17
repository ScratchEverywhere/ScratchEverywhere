#pragma once
#ifndef _XBOX_BCRYPT_H
#define _XBOX_BCRYPT_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#endif

#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(status) (((long)(status)) >= 0)
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((long)0x00000000)
#endif

typedef void *BCRYPT_ALG_HANDLE;

extern int rand_s(unsigned int *randomValue);

static inline long BCryptGenRandom(BCRYPT_ALG_HANDLE hAlgorithm, unsigned char *pbBuffer, unsigned long cbBuffer, unsigned long dwFlags) {
    (void)hAlgorithm;
    (void)dwFlags;
    if (!pbBuffer) return -1;
    unsigned long i = 0;
    while (i < cbBuffer) {
        unsigned int val = 0;
        rand_s(&val);
        unsigned long chunk = cbBuffer - i;
        if (chunk > sizeof(val)) chunk = sizeof(val);
        memcpy(pbBuffer + i, &val, chunk);
        i += chunk;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _XBOX_BCRYPT_H */
