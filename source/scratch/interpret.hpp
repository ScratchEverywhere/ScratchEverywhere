#pragma once
#include "blockExecutor.hpp"
#include "image.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <list>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <time.hpp>
#include <unordered_map>
#include <vector>

enum ProjectType {
    UNZIPPED,
    EMBEDDED,
    UNEMBEDDED
};

class BlockExecutor;
extern BlockExecutor executor;

extern ProjectType projectType;

extern std::vector<Sprite *> sprites;
extern std::vector<Sprite> spritePool;
extern std::vector<std::string> broadcastQueue;
// extern std::unordered_map<std::string,Conditional> conditionals;
extern std::unordered_map<std::string, Block *> blockLookup;
extern bool toExit;
extern std::string answer;

class Scratch {
  public:
    static Value getInputValue(Block &block, const std::string &inputName, Sprite *sprite);

    static int projectWidth;
    static int projectHeight;
    static int FPS;
};

std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite);
void loadSprites(const nlohmann::json &json);
void cleanupSprites();
Block *getBlockParent(const Block *block);
void initializeSpritePool(int poolSize);
Sprite *getAvailableSprite();
void initializeSpritePool(int poolSize);
Value findCustomValue(std::string valueName, Sprite *sprite, Block block);
std::string removeQuotations(std::string value);
void runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh);
Block *findBlock(std::string blockId);
std::vector<Sprite *> findSprite(std::string spriteName);

Value getVariableValue(std::string variableId, Sprite *sprite);
void setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite);
std::string generateRandomString(int length);
std::vector<Block *> getBlockChain(std::string blockId, std::string *outID = nullptr);
