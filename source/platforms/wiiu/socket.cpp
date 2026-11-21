#include <arpa/inet.h>
#include <coreinit/time.h>
#include <cstddef>
#include <cstdint>

static bool se_wiiuRandom(void *buf, size_t len) {
    static uint32_t counter = 0;
    uint64_t state = (uint64_t)OSGetSystemTime() ^
                     ((uint64_t)OSGetSystemTick() << 32) ^
                     ((uint64_t)__atomic_add_fetch(&counter, 1, __ATOMIC_RELAXED) * 0x9E3779B97F4A7C15ULL);
    state |= 1;

    uint8_t *out = static_cast<uint8_t *>(buf);
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = (uint8_t)((state * 0x2545F4914F6CDD1DULL) >> 56);
    }
    return true;
}
#define SE_NET_RANDOM(buf, len) se_wiiuRandom(buf, len)

#define SE_NET_USE_GETHOSTBYNAME

#include <socket_bsd.hpp>
