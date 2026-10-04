#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct Sb2ArgSpec {
    enum class Kind { Input,
                      Field } kind;
    std::string name;
    std::string inputOp;

    std::string variableType;
};

struct Sb2BlockSpec {
    std::string opcode;
    std::vector<Sb2ArgSpec> argMap;
};

const Sb2BlockSpec *lookupSb2Spec(const std::string &oldOpcode);
