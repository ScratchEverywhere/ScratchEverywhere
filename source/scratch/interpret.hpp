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
extern std::unordered_map<std::string, Block *> blockLookup;
extern bool toExit;
extern std::string answer;

class Scratch {
  public:
    static Value getInputValue(Block &block, const std::string &inputName, Sprite *sprite);

    static void fenceSpriteWithinBounds(Sprite *sprite);

    static int projectWidth;
    static int projectHeight;
    static int FPS;
    static bool fencing;
    static bool miscellaneousLimits;
};

/**
 * Gets the Sprite's box collision points.
 * @param sprite
 * @return Each point stored in a `std::pair`, where `[0]` is X, `[1]` is Y.
 */
std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite);

/**
 * Loads every Sprite from the Scratch's project.json file.
 * @param json The file to load
 */
void loadSprites(const nlohmann::json &json);

/**
 * Frees every Sprite from memory.
 */
void cleanupSprites();

/**
 * Gets the top level block of the specified `Block`.
 * @param block
 * @return The top level parent of the specified `block`.
 */
Block *getBlockParent(const Block *block);

/**
 * Initializes a Pool of `Sprite` variables to be used by Clones.
 * @param poolSize Amount of clones to initialize.
 */
void initializeSpritePool(int poolSize);

/**
 * Gets an available sprite from the `Sprite Pool`.
 * @return A `Sprite*` if there is any, `nullptr` otherwise.
 */
Sprite *getAvailableSprite();

/**
 * Finds a block from the `blockLookup`.
 * @param blockId ID of the block you need
 * @return A `Block*` if it's found, `nullptr` otherwise.
 */
Block *findBlock(std::string blockId);

/**
 * Gets a Chain of Blocks with a specified `blockId`.
 * @param blockId ID of the block you want the chain for.
 * @param outId a `std::string*` for if you want to get the ID of the chain. Can leave empty.
 * @return An `std::vector` of every `Block*` in the chain.
 */
std::vector<Block *> getBlockChain(std::string blockId, std::string *outID = nullptr);
