
// runtime.hpp
#pragma once
#include "input.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <time.hpp>
#include <unordered_map>
#include <vector>

class Scratch {
  public:
    static enum ProjectLocation {
        EMBEDDED,
        UNEMBEDDED
    } projectLocation;
    static enum ProjectType {
        EXTRACTED,
        ARCHIVE
    } projectType;

    static int projectWidth;
    static int projectHeight;
    static int FPS;
    static bool turbo;
    static bool fencing;
    static bool hqpen;
    static bool miscellaneousLimits;
    static bool forceRedraw;

    static double counter;

    static bool nextProject;
    static Value dataNextProject;

    static bool toExit;
};

struct Broadcast;
struct Sprite;
struct Block;

class Runtime {
  public:
    static void registerHandlers();
    static void unloadHandlers();

    static void loadScratchProject();

    static bool startScratchProject();

    static unsigned int blockCounter;
    static std::unordered_map<int, std::unique_ptr<Block>> blocks;
    std::unique_ptr<Sprite> stage;
    static std::vector<std::unique_ptr<Sprite>> sprites;

    Sprite &cloneSprite(Sprite &original);

  private:
    static std::unordered_map<std::string, unsigned int> costumeHatBlockIDs; // thats only needed while loading the project
    static std::vector<std::string> costumeHatBlocks;
    static std::unordered_map<std::string, std::function<BlockResult(Block *block, ScriptThread *thread, Sprite *sprite)>> handlers;
    static std::unordered_map<std::string, std::function<Value(const nlohmann::json &field)>> fieldHandlers;
    static void loadSprites();
    static void loadBlocks(Sprite &newSprite, const nlohmann::json &spriteData);
    static unsigned int loadBlock(Sprite &newSprite, const std::string id, const nlohmann::json &blockDatas);
    static void loadInputs(Block &block, Sprite &newSprite, std::string blockID, const nlohmann::json &blockDatas);
    static void loadFields(Block &block, std::string blockID, const nlohmann::json &blockDatas);
    static void loadMonitorVariables();
    static void loadAdvancedProjectSettings();
    static void sortSprites();
};