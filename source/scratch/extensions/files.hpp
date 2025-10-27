#include <functional>
#include <string>
#include <vector>

namespace extensions::files {
// These return callbacks to the functions callable by Lua.
std::function<std::string(const std::string path)> read(bool rootfs, std::string extensionId);
std::function<void(const std::string path, const std::string content)> write(bool rootfs, std::string extensionId);
std::function<void(const std::string path, const std::string content)> append(bool rootfs, std::string extensionId);
std::function<void(const std::string path)> mkdir(bool rootfs, std::string extensionId);
std::function<std::vector<std::string>(const std::string path)> ls(bool rootfs, std::string extensionId);

// Can't use `OS::getScratchFolderLocation` because this path is already relative to the roots.
#ifdef __WIIU__
const std::string mainDirectory = "/wiiu/scratch-wiiu/";
#elif defined(__SWITCH__)
const std::string mainDirectory = "/switch/scratch-nx/";
#elif defined(WII)
const std::string mainDirectory = "/apps/scratch-wii/";
#elif defined(GAMECUBE)
const std::string mainDirectory = "/scratch-gamecube/";
#elif defined(VITA)
const std::string mainDirectory = "/scratch-vita/";
#elif defined(__3DS__)
const std::string mainDirectory = "/3ds/scratch-everywhere/";
#else
const std::string mainDirectory = "scratch-everywhere/";
#endif
} // namespace extensions::files
