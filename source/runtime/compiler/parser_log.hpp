#include <string>

#include "../../log.hpp"
#include "../core/value.hpp"

namespace ParserLog {
inline void logFold(std::string blockName, std::string inputName) {
    Log::log(std::string("[PARSER] FOLD | ") + blockName + std::string(" | ") + inputName);
}
inline void log(std::string block) {
    Log::log(block);
}
} // namespace ParserLog