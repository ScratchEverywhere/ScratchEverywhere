#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "jit_probe.hpp"
#include "jit_probe_core.hpp"
#include <log.hpp>
#include <string>

namespace BytecodeJitInternal {

bool backendUsable() {
    static const bool usable = probeBackend([](const char *message, const char *detail) {
        Log::log(std::string("[jit] ") + message + (detail != nullptr ? std::string(": ") + detail : std::string()));
    });
    return usable;
}

} // namespace BytecodeJitInternal

#endif
