// These are missing
// They make the app not link
#ifdef __PS3__
#include <errno.h>

int getentropy(void *buf, size_t len) {
    errno = ENOSYS;
    return -1;
}

int openat(int dirfd, const char *path, int flags, ...) {
    errno = ENOSYS;
    return -1;
}

void *fdopendir(int fd) {
    errno = ENOSYS;
    return NULL;
}

int unlinkat(int dirfd, const char *pathname, int flags) {
    errno = ENOSYS;
    return -1;
}
#endif