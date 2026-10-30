/*
 * Automatically redirect stdout to WHBLogWrite().
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * SPDX-License-Identifier: MIT
 *
 * Source: https://github.com/dkosmari/wiiu-stdout
 */

#ifdef __WIIU__

#include <mutex>
#include <optional>
#include <string>

#include <sys/iosupport.h>      // devoptab_list, devoptab_t

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>


std::optional<std::mutex> whb_log_mutex; // initialized by stderr code.


namespace {

    bool cafe_initialized   = false;
    bool module_initialized = false;
    bool udp_initialized    = false;

}


__attribute__ (( __constructor__ (101) ))
void
init_whb_log()
    noexcept
{
    module_initialized = WHBLogModuleInit();
    if (!module_initialized) {
        cafe_initialized = WHBLogCafeInit();
        udp_initialized = WHBLogUdpInit();
    }
}


__attribute__ (( __destructor__ (101) ))
void
fini_whb_log()
    noexcept
{
    if (module_initialized) {
        WHBLogModuleDeinit();
        module_initialized = false;
    }
    if (cafe_initialized) {
        WHBLogCafeDeinit();
        cafe_initialized = false;
    }
    if (udp_initialized) {
        WHBLogUdpDeinit();
        udp_initialized = false;
    }
}


ssize_t
devoptab_to_whb_log(struct _reent*,
                    void*,
                    const char* buf,
                    size_t len)
    noexcept
{
    try {
        // Note: WHBLogWrite expects a null-terminated string.
        std::string msg(buf, len);

        if (whb_log_mutex) {
            std::lock_guard guard{*whb_log_mutex};
            WHBLogWrite(msg.data());
        } else {
            WHBLogWrite(msg.data());
        }
        return len;
    }
    catch (...) {
        return -1;
    }
}


__attribute__(( __constructor__ (102) ))
void
init_stdout()
{
    static devoptab_t stdout_dev;
    stdout_dev.name = "STDOUT";
    stdout_dev.structSize = sizeof stdout_dev;
    stdout_dev.write_r = devoptab_to_whb_log;
    devoptab_list[STD_OUT] = &stdout_dev;
}

#endif // __WIIU__
