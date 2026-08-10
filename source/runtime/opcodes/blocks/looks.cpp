#include "../opcode_registers.hpp"

REGISTER_FIELD_DISPATCH_PARSER(
    "looks_costumenumbername",
    "NUMBERNAME",
    Purity::Impure,
    (std::vector<std::pair<std::string, uint16_t>>{
        {"number", Opcode::looks_costumenumbername_number},
        {"name", Opcode::looks_costumenumbername_name}}));