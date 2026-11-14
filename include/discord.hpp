#pragma once

#include <string>

namespace Discord {

#ifdef ENABLE_DISCORD_RPC
void init();
void shutdown();
void setInMenu();
void setPlaying(const std::string &projectPath);
bool isEnabled();
void setEnabled(bool enabled);
#else
inline void init() {}
inline void shutdown() {}
inline void setInMenu() {}
inline void setPlaying(const std::string &) {}
inline bool isEnabled() { return false; }
inline void setEnabled(bool) {}
#endif

} // namespace Discord
