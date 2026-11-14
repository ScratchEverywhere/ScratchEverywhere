#include <switch.h>

static bool se_nxRandom(void *buf, size_t len) {
    if (R_FAILED(csrngInitialize())) return false;
    csrngGetRandomBytes(buf, len);
    csrngExit();
    return true;
}
#define SE_NET_RANDOM(buf, len) se_nxRandom(buf, len)

#include <socket_bsd.hpp>
