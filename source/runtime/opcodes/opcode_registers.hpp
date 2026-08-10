#pragma once
#include "../compiler/compiler_context.hpp"
#include "../core/vm_types.hpp"
#include "opcodes.hpp"

struct HatParseResult {
    HatType hatType;
    uint16_t eventParamId = 0;
    bool isValid = true;
};

using BlockHandler = BlockResult (*)(VMThread *thread);
using ParserHandler = CompileResult (*)(CompilerContext &ctx);
using HatParserHandler = HatParseResult (*)(CompilerContext &ctx, const nlohmann::json &blockJson);

enum class Purity {
    Pure,
    Impure
};

class ParserRegistry {
  public:
    static std::unordered_map<std::string, ParserHandler> &getParserMap() {
        static std::unordered_map<std::string, ParserHandler> map;
        return map;
    }
    static bool registerParser(const std::string &scratchName, ParserHandler handler);
    static CompileResult compileStandard(CompilerContext &ctx, uint16_t opcode, const std::vector<std::string> &inputs, Purity purity);
};

class OpcodeRegistry {
  public:
    static std::array<BlockHandler, MAX_BLOCKS> &getJumpTable() {
        static std::array<BlockHandler, MAX_BLOCKS> map;
        return map;
    }
    static bool registerBlock(uint16_t opcode, BlockHandler handler);
    static BlockResult executeByteCode(VMThread *thread);
};

class HatBlockRegistry {
  public:
    static std::unordered_map<std::string, HatParserHandler> &getHatMap() {
        static std::unordered_map<std::string, HatParserHandler> map;
        return map;
    }

    static bool registerHat(const std::string &scratchOpcode, HatParserHandler handler) {
        getHatMap()[scratchOpcode] = handler;
        return true;
    }
};

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#define DEFINE_EXECUTION(name)                                                                                 \
    BlockResult exec_##name(VMThread *thread);                                                                 \
    namespace {                                                                                                \
    static const bool CONCAT(reg_exec_, __LINE__) = OpcodeRegistry::registerBlock(Opcode::name, &exec_##name); \
    }                                                                                                          \
    BlockResult exec_##name(VMThread *thread)

#define REGISTER_STANDARD_PARSER(scratchStringName, opcodeEnum, ...)                         \
    namespace {                                                                              \
    static const bool CONCAT(reg_parse_, __LINE__) = ParserRegistry::registerParser(         \
        scratchStringName,                                                                   \
        [](CompilerContext &ctx) -> CompileResult {                                          \
            static const std::vector<std::string> inputs = {__VA_ARGS__};                    \
            return ParserRegistry::compileStandard(ctx, opcodeEnum, inputs, Purity::Impure); \
        });                                                                                  \
    }

#define DEFINE_CUSTOM_PARSER(scratchStringName, funcId)                                                                         \
    CompileResult parse_##funcId(CompilerContext &ctx);                                                                         \
    namespace {                                                                                                                 \
    static const bool CONCAT(reg_custom_parse_, __LINE__) = ParserRegistry::registerParser(scratchStringName, &parse_##funcId); \
    }                                                                                                                           \
    CompileResult parse_##funcId(CompilerContext &ctx)

#define REGISTER_FIELD_DISPATCH_PARSER(scratchStringName, keyName, purity, mappingMap, ...)         \
    namespace {                                                                                     \
    static const bool CONCAT(reg_dispatch_parse_, __LINE__) =                                       \
        ParserRegistry::registerParser(                                                             \
            scratchStringName,                                                                      \
            [](CompilerContext &ctx) -> CompileResult {                                             \
                std::string val = ctx.resolveFieldValue(keyName);                                   \
                                                                                                    \
                static const std::vector<std::pair<std::string, uint16_t>> mappingVec = mappingMap; \
                static const std::vector<std::string> inputs = {__VA_ARGS__};                       \
                                                                                                    \
                uint16_t targetOpcode = 0;                                                          \
                if (!mappingVec.empty()) {                                                          \
                    targetOpcode = mappingVec.front().second;                                       \
                }                                                                                   \
                for (const auto &pair : mappingVec) {                                               \
                    if (pair.first == val) {                                                        \
                        targetOpcode = pair.second;                                                 \
                        break;                                                                      \
                    }                                                                               \
                }                                                                                   \
                                                                                                    \
                return ParserRegistry::compileStandard(ctx, targetOpcode, inputs, purity);          \
            });                                                                                     \
    }

#define REGISTER_SIMPLE_HAT(scratchOpcode, hatTypeEnum)                           \
    namespace {                                                                   \
    static const bool CONCAT(reg_hat_, __LINE__) = HatBlockRegistry::registerHat( \
        scratchOpcode,                                                            \
        [](CompilerContext &ctx, const nlohmann::json &block) -> HatParseResult { \
            return {hatTypeEnum, 0, true};                                        \
        });                                                                       \
    }

#define DEFINE_HAT_PARSER(scratchOpcode, funcId)                                                                             \
    HatParseResult parse_hat_##funcId(CompilerContext &ctx, const nlohmann::json &blockJson);                                \
    namespace {                                                                                                              \
    static const bool CONCAT(reg_custom_hat_, __LINE__) = HatBlockRegistry::registerHat(scratchOpcode, &parse_hat_##funcId); \
    }                                                                                                                        \
    HatParseResult parse_hat_##funcId(CompilerContext &ctx, const nlohmann::json &blockJson)