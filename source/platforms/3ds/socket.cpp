#include <3ds.h>

static bool se_3dsRandom(void *buf, size_t len) {
    if (R_FAILED(psInit())) return false;
    Result res = PS_GenerateRandomBytes(buf, len);
    psExit();
    return R_SUCCEEDED(res);
}
#define SE_NET_RANDOM(buf, len) se_3dsRandom(buf, len)

#include <socket_bsd.hpp>
