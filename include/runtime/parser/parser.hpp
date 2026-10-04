#pragma once
#include <nlohmann/json.hpp>
#include <se_export.hpp>
#include <types.hpp>
#include <unordered_map>

enum class SE_EXPORT ProjectFormat {
    SB3,
    SB2,
    SB1,
};

struct SE_EXPORT Parser {
    static std::unordered_map<std::string, std::string> &getShadowBlocks();

    static bool logParsing;

    static void loadUsernameFromSettings();

    // Only for JSON-shaped formats (SB3, SB2)
    static void loadSprites(const nlohmann::json &json, ProjectFormat format = ProjectFormat::SB3);
    static bool loadExtensions(const nlohmann::json &json);

#ifdef ENABLE_CLOUDVARS
    static void initMist();
#endif
  private:
    static void log(const std::string &message);

    static void loadSpritesSb3(const nlohmann::json &json);
    static Block *loadBlockSb3(Sprite *newSprite, const std::string &id, const nlohmann::json &blockDatas, Block *parentBlock, int indent);
    static void loadFieldsSb3(Block &block, const std::string &blockKey, const nlohmann::json &blockDatas, int indent);
    static void loadInputsSb3(Block &block, Sprite *newSprite, const std::string &blockKey, const nlohmann::json &blockDatas, int indent);
    static void setSubstackSb3(Block *startBlock, Block *stopBlock = nullptr);
    static void loadAdvancedProjectSettingsSb3(const nlohmann::json &json);
    static void resolveVariableTypes(Sprite *sprite);

    static void loadSpritesSb2(const nlohmann::json &json);
    static void parseSb2Scripts(Sprite *sprite, const nlohmann::json &object);
};
