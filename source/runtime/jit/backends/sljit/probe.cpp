#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "probe_core.hpp"
#include <log.hpp>
#include <string>

namespace BytecodeJitInternal {

bool sljitProbe() {
    static const bool usable = probeBackend([](const char *message, const char *detail) {
        Log::log(std::string("[jit] ") + message + (detail != nullptr ? std::string(": ") + detail : std::string()));
    });
#if defined(__XBOX__)
    static bool probeLogged = false;
    if (!probeLogged) {
        probeLogged = true;
        Log::log(std::string("[jit probe] SLJIT backend probe: ") + (usable ? "PASS (x87 backend usable)" : "FAIL"));
    }
#endif
    return usable;
}

} // namespace BytecodeJitInternal

#endif
