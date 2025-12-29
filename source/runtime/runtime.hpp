#pragma once
#include "blockExecutor.hpp"
#include "sprite.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <time.hpp>
#include <unordered_map>
#include <vector>

#ifdef __3DS__
#include "speech_manager_c2d.hpp"
#elif defined(RENDERER_SDL1)
#include "speech_manager_sdl1.hpp"
#elif defined(RENDERER_SDL2)
#include "speech_manager_sdl2.hpp"
#elif defined(RENDERER_SDL3)
#include "speech_manager_sdl3.hpp"
#elif defined(__NDS__)
#include "speech_manager_gl2d.hpp"
#else
#include "speech_manager.hpp"
#endif

enum ProjectType {
    UNZIPPED,
    EMBEDDED,
    UNEMBEDDED
};

class BlockExecutor;
extern BlockExecutor executor;

#ifdef __3DS__
class SpeechManagerC2D;
extern SpeechManagerC2D *speechManager;
#elif defined(RENDERER_SDL1)
class SpeechManagerSDL1;
extern SpeechManagerSDL1 *speechManager;
#elif defined(RENDERER_SDL2)
class SpeechManagerSDL2;
extern SpeechManagerSDL2 *speechManager;
#elif defined(RENDERER_SDL3)
class SpeechManagerSDL3;
extern SpeechManagerSDL3 *speechManager;
#elif defined(__NDS__)
class SpeechManagerGL2D;
extern SpeechManagerGL2D *speechManager;
#else
class SpeechManager;
extern SpeechManager *speechManager;
#endif

class Scratch {
  public:
    static bool startScratchProject();
    static void cleanupScratchProject();

    static std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);

    static Value getInputValue(Block &block, const std::string &inputName, Sprite *sprite);
    static std::string getFieldValue(Block &block, const std::string &fieldName);
    static std::string getFieldId(Block &block, const std::string &fieldName);

    /**
     * Gets the top level block of the specified `Block`.
     * @param block
     * @param sprite
     * @return The top level parent of the specified `block`.
     */
    static Block *getBlockParent(const Block *block, Sprite* sprite);

    /**
     * Finds a block from a sprite.
     * @param blockId ID of the block you need
     * @param sprite The sprite to limit the search to.
     * @return A `Block*` if it's found, `nullptr` otherwise.
     */
    static Block *findBlock(std::string blockId, Sprite* sprite);

    /**
     * Gets the Sprite's box collision points.
     * @param sprite
     * @return Each point stored in a `std::pair`, where `[0]` is X, `[1]` is Y.
     */
    static std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite);

    /**
     * Frees every Sprite from memory.
     */
    static void cleanupSprites();

    static void gotoXY(Sprite *sprite, double x, double y);
    static void fenceSpriteWithinBounds(Sprite *sprite);
    static bool isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite = nullptr, std::string targetName = "");
    static void switchCostume(Sprite *sprite, double costumeIndex);
    static void setDirection(Sprite *sprite, double direction);
    static Sprite *getListTargetSprite(std::string listId, Sprite *sprite);
    static void sortSprites();

    static int projectWidth;
    static int projectHeight;
    static int FPS;
    static int cloneCount;
    static int maxClones;
    static bool turbo;
    static bool fencing;
    static bool hqpen;
    static bool miscellaneousLimits;
    static bool shouldStop;
    static bool forceRedraw;

    static double counter;

    static bool nextProject;
    static Value dataNextProject;

    static std::vector<Sprite *> sprites;
    static Sprite *stageSprite;
    static std::vector<std::string> broadcastQueue;
    static std::vector<Sprite *> cloneQueue;
    static std::string answer;
    static ProjectType projectType;

    static bool useCustomUsername;
    static std::string customUsername;

#ifdef ENABLE_CLOUDVARS
    static bool cloudProject;
    static std::string cloudUsername;
#endif
};
