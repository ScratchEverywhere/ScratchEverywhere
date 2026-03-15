#pragma once
#include <nlohmann/json.hpp>
#include <sprite.hpp>

struct Parser {

    static std::unordered_map<std::string, Block *> costumeHatBlock;
    static bool logParsing;

    static void loadUsernameFromSettings();

    static void loadSprites(const nlohmann::json &json);

#ifdef ENABLE_CLOUDVARS
    static void initMist();
#endif
  private:
    static void log(const std::string &message);

    static Block *loadBlock(Sprite *newSprite, const std::string id, const nlohmann::json &blockDatas, Block *parentBlock, int indent);
    static void loadFields(Block &block, std::string blockKey, const nlohmann::json &blockDatas, int indent);
    static void loadInputs(Block &block, Sprite *newSprite, std::string blockKey, const nlohmann::json &blockDatas, int indent);
    static void loadAdvancedProjectSettings(const nlohmann::json &json);

}; // namespace Parser