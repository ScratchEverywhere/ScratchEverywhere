#pragma once

#include <cstdint>
#include <vector>

#include "../../log.hpp"
#include "../core/value.hpp"
#include <render.hpp>
#include <timer.hpp>

#ifdef ENABLE_MENU
#include "menus/pauseMenu.hpp"
#endif

class ConstantPool {
  public:
    static std::vector<Value> &getPool() {
        static std::vector<Value> pool;
        return pool;
    }

    static uint16_t getOrInsert(const Value &val) { // I hope that uint16_t are enough constants. (65536 const values)
        auto &pool = getPool();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (pool[i].strictEquals(val)) {
                return static_cast<uint16_t>(i);
            }
        }
        if (pool.size() == UINT16_MAX) {
            Log::logCritical("Constant pool limit reached (> 65535 unique constants). Cannot emit constant.", true);
            return UINT16_MAX;
        }
        uint16_t index = static_cast<uint16_t>(pool.size());
        pool.push_back(val);
        return index;
    }

    static const Value &get(uint16_t index) {
        return getPool()[index];
    }

    static const std::vector<Value> &getValues() { return getPool(); }
    static Value getCopy(uint16_t idx) { return getPool()[idx]; }
    static void clear() { getPool().clear(); }
};

enum class ProjectType {
    UNZIPPED,
    EMBEDDED,
    UNEMBEDDED
};

namespace EngineState {
extern bool exportBytecode;
extern uint32_t dispatchId;
extern int fps;
extern Timer fpsTimer;
extern Timer timer;
extern int projectWidth, projectHeight;
extern int maxClones;
extern float tempo;
extern bool warpTimer;
extern bool turbo;
extern bool fencing;
extern bool hqPen;
extern bool accuratePen;
extern bool accurateCollision;
extern bool miscellaneousLimits;
extern bool warpTimer;
extern bool bitmapHalfQuality;
extern bool hasNativeExtensions;
extern bool sb3InRam;
extern ProjectType projectType;
extern std::string answer;

extern bool useCustomUsername;
extern std::string customUsername;

extern bool forceRedraw;
extern bool shouldStop;

extern bool nextProject;
extern Value dataNextProject;

#ifdef ENABLE_CLOUDVARS
extern bool cloudProject;
#endif

#ifdef ENABLE_MENU
extern PauseMenu *pauseMenu;
#endif

extern bool debugVars;

static inline void resetToDefaults() {
    answer.clear();
    customUsername.clear();
    projectWidth = 480;
    projectHeight = 360;
    maxClones = 300;
    fps = 30;
    hasNativeExtensions = false;
    turbo = false;
    hqPen = false;
    fencing = true;
    miscellaneousLimits = true;
    shouldStop = false;
    forceRedraw = false;
    useCustomUsername = false;
    sb3InRam = true;
    warpTimer = true;
    projectType = ProjectType::UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;
}
} // namespace EngineState