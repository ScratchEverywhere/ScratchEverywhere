#pragma once
#include "blockExecutor.hpp"
#include "sprite.hpp"
#include <image.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <time.hpp>
#include <unordered_map>
#include <vector>

enum class ProjectType {
    UNZIPPED,
    EMBEDDED,
    UNEMBEDDED
};

#ifdef ENABLE_MENU
class PauseMenu;
#endif
class BlockExecutor;
extern BlockExecutor executor;

class Scratch {
  public:
    static void initializeScratchProject();
    static bool getInput(Block *block, std::string inputName, ScriptThread *thread, Sprite *sprite, Value &outValue);
    static void resetInput(Block *block, std::string inputName = "");

    /**
     * Runs a single step of execution.
     * @return First bool for if loop should be continued, second bool for exit code
     */
    static std::pair<bool, bool> stepScratchProject(ScriptThread &monitorDisplayThread);
    static bool startScratchProject();
    static void cleanupScratchProject();

    static void greenFlagClicked();
    static void stopClicked();

    static std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);

    static std::string getFieldValue(Block &block, const std::string &fieldName);
    static std::string getFieldId(Block &block, const std::string &fieldName);
    static std::string getListName(Block &block);
    static std::vector<Value> *getListItems(Block &block, Sprite *sprite);

    /**
     * Frees every Sprite from memory.
     */
    static void cleanupSprites();

    static void gotoXY(Sprite *sprite, double x, double y);
    static void fenceSpriteWithinBounds(Sprite *sprite);
    static bool isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite = nullptr, std::string targetName = "");
    static void switchCostume(Sprite *sprite, double costumeIndex);
    static void setDirection(Sprite *sprite, double direction);
    static void sortSprites();
    static void moveLayer(Sprite *s, int layers);

    static std::unordered_map<std::string, std::shared_ptr<Image>> costumeImages;
    static void loadCurrentCostumeImage(Sprite *sprite);
    static void flushCostumeImages();
    static void freeUnusedCostumeImages();

    static void createDebugMonitor(const std::string &name, int x, int y);
    static void toggleDebugVars(const bool enabled);

    static bool hasNativeExtensions;

    static int projectWidth;
    static int projectHeight;
    static int FPS;
    static int cloneCount;
    static int maxClones;
    static bool turbo;
    static bool fencing;
    static bool hqpen;
    static bool accuratePen;
    static bool accurateCollision;
    static bool miscellaneousLimits;
    static bool shouldStop;
    static bool forceRedraw;
    static bool warpTimer;

#if defined(__NDS__) || defined(GAMECUBE) || defined(__PSP__)
    constexpr static bool bitmapHalfQuality = true;
#else
    constexpr static bool bitmapHalfQuality = false;
#endif

    static bool debugVars;
    static bool sb3InRam;

    static double counter;

    static bool nextProject;
    static Value dataNextProject;
    static std::string newBroadcast;

    static Timer fpsTimer;

#ifdef ENABLE_MENU
    static PauseMenu *pauseMenu;
#endif

    static std::vector<Sprite *> sprites;
    static Sprite *stageSprite;
    static std::vector<Block *> blocks;
    static std::string answer;
    static ProjectType projectType;

    static bool useCustomUsername;
    static std::string customUsername;

#ifdef ENABLE_CLOUDVARS
    static bool cloudProject;
    static std::string cloudUsername;
#endif
};
