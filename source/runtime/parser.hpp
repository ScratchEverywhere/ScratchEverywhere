#pragma once
#include <nlohmann/json.hpp>
#include <sprite.hpp>

struct Parser {

    static bool logParsing;

    static void loadUsernameFromSettings();

    static void loadSprites(const nlohmann::json &json);
    static bool loadExtensions(const nlohmann::json &json);

#ifdef ENABLE_CLOUDVARS
    static void initMist();
#endif
  private:
    static void log(const std::string &message);

    static Block *loadBlock(Sprite *newSprite, const std::string id, const nlohmann::json &blockDatas, Block *parentBlock, int indent);
    static void loadFields(Block &block, const std::string &blockKey, const nlohmann::json &blockDatas, int indent);
    static void loadInputs(Block &block, Sprite *newSprite, std::string blockKey, const nlohmann::json &blockDatas, int indent);
    static void loadAdvancedProjectSettings(const nlohmann::json &json);
    static void setSubstack(Block *startBlock, Block *stopBlock = nullptr);

}; // namespace Parser
