#pragma once
#include <string>
#if defined(__XBOX__) && defined(SE_XBOX_TRACE_MENU_INPUT)
#include <SDL3/SDL_timer.h>
#include <input.hpp>
#include <log.hpp>

namespace XboxMenuTrace {
inline constexpr bool enabled = true;
inline unsigned gameFrames = 0;
inline void snapshot(const std::string &event) {
    std::string line = "Menu input " + std::to_string(SDL_GetTicks()) + " " + event + " buttons=";
    for (const auto &button : Input::inputButtons) line += button + ",";
    line += " keys=";
    for (const auto &key : Input::inputKeys) line += key + ",";
    for (const auto *key : {"a", "x", "up arrow", "space"}) {
        auto it = Input::keyHeldDuration.find(key);
        line += " [" + std::string(key) + "=" + std::to_string(it == Input::keyHeldDuration.end() ? 0 : it->second) + "]";
    }
    line += " mouse=" + std::to_string(Input::mousePointer.isPressed);
    line += " stage=" + std::to_string(Scratch::stageSprite != nullptr);
#ifdef ENABLE_MENU
    line += " paused=" + std::to_string(Scratch::pauseMenu != nullptr);
#endif
    Log::log(line);
}
inline void beginGameFrames() { gameFrames = 8; }
inline void gameFrame() {
    if (gameFrames) { --gameFrames; snapshot("post-handoff poll"); }
}
}
#else
namespace XboxMenuTrace {
inline constexpr bool enabled = false;
inline void snapshot(const std::string &) {}
inline void beginGameFrames() {}
inline void gameFrame() {}
}
#endif
