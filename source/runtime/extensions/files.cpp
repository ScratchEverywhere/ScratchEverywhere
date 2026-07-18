#include "files.hpp"
#include "filesystem.hpp"
#include "log.hpp"
#include "meta.hpp"
#include "os.hpp"
#include <fstream>

namespace extensions::files {
struct FilesWrapper {
    std::vector<ExtensionPermission> permissions;

    std::string localRoot;

    FilesWrapper(const Extension *ext) {
        const std::string extensionDataRoot = OS::getScratchFolderLocation() + "extension-data/";
        localRoot = extensionDataRoot + ext->id + "/";
        permissions = ext->permissions;

        if (!hasPermission(ExtensionPermission::LOCALFS)) return;
        if (!FileSystem::fileExists(extensionDataRoot)) {
            if (!FileSystem::createDirectory(extensionDataRoot).has_value()) {
                Log::logError("Failed to create extension data directory.");
                return;
            }
        }
        if (!FileSystem::fileExists(localRoot)) {
            if (!FileSystem::createDirectory(localRoot).has_value()) {
                Log::logError("Failed to create extension data directory for: " + ext->id);
                return;
            }
        }
    }

    bool hasPermission(ExtensionPermission perm) const {
        return std::find(permissions.begin(), permissions.end(), perm) != permissions.end();
    }

    std::string sanitizeAndResolvePath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');

        if (path.find('~') != std::string::npos) {
            return "";
        }

        bool forceLocal = false;
        if (path.rfind("./", 0) == 0 || path == ".") {
            forceLocal = true;
        }

        std::vector<std::string> segments;
        std::stringstream ss(path);
        std::string segment;

        while (std::getline(ss, segment, '/')) {
            if (segment == "" || segment == ".") {
                continue;
            }

            if (segment == "..") {
                if (segments.empty()) {
                    return "";
                }
                segments.pop_back();
            } else {
                segments.push_back(segment);
            }
        }

        std::string finalPath;
        for (const auto &seg : segments) {
            if (!finalPath.empty()) {
                finalPath += "/";
            }
            finalPath += seg;
        }

        std::string selectedRoot = "";

        if (hasPermission(ExtensionPermission::LOCALFS) && hasPermission(ExtensionPermission::ROOTFS)) {
            selectedRoot = forceLocal ? localRoot : OS::getFilesystemRootPrefix();
        } else if (hasPermission(ExtensionPermission::ROOTFS)) {
            selectedRoot = OS::getFilesystemRootPrefix();
        } else {
            selectedRoot = localRoot;
        }

        return selectedRoot + finalPath;
    }

    sol::object getMainDirectory(sol::this_state s) {
        if (!hasPermission(ExtensionPermission::ROOTFS)) {
            return sol::lua_nil;
        }
        return sol::make_object(s, OS::getScratchFolderLocation());
    }

    sol::object read(const std::string &path, sol::this_state s) {
        const std::string fullPath = sanitizeAndResolvePath(path);
        if (fullPath.empty()) return sol::lua_nil;

        std::ifstream file(fullPath, std::ios::in | std::ios::binary);
        if (!file.is_open()) return sol::lua_nil;

        const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return sol::make_object(s, content);
    }

    bool write(const std::string &path, const std::string &content) {
        const std::string fullPath = sanitizeAndResolvePath(path);
        if (fullPath.empty()) return false;

        std::ofstream file(fullPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;

        file.write(content.c_str(), content.size());
        return true;
    }

    bool append(const std::string &path, const std::string &content) {
        const std::string fullPath = sanitizeAndResolvePath(path);
        if (fullPath.empty()) return false;

        std::ofstream file(fullPath, std::ios::out | std::ios::binary | std::ios::app);
        if (!file.is_open()) return false;

        file.write(content.c_str(), content.size());
        return true;
    }

    bool mkdir(const std::string &path) {
        const std::string fullPath = sanitizeAndResolvePath(path);
        if (fullPath.empty()) return false;

        return FileSystem::createDirectory(fullPath).has_value();
    }

    sol::object ls(const std::string &path, sol::this_state s) {
        const std::string fullPath = sanitizeAndResolvePath(path);
        if (fullPath.empty()) return sol::lua_nil;

        sol::state_view luaState = s;
        sol::table luaTable = luaState.create_table();

        auto files = FileSystem::listDirectory(fullPath);
        if (!files.has_value()) {
            Log::logError("Error while reading directory, " + fullPath + ": " + files.error());
            return sol::lua_nil;
        }

        return sol::make_object(luaState, files.value());
    }
};

void registerAPI(Extension *extension) {
    if (!extension->hasPermission(ExtensionPermission::LOCALFS) &&
        !extension->hasPermission(ExtensionPermission::ROOTFS)) {
        return;
    }

    const auto wrapper = std::make_shared<FilesWrapper>(extension);

    extension->luaState["files"] = extension->luaState.create_table();

    extension->luaState["files"]["read"] = [wrapper](const std::string &path, sol::this_state s) {
        return wrapper->read(path, s);
    };
    extension->luaState["files"]["write"] = [wrapper](const std::string &path, const std::string &content) {
        return wrapper->write(path, content);
    };
    extension->luaState["files"]["append"] = [wrapper](const std::string &path, const std::string &content) {
        return wrapper->append(path, content);
    };
    extension->luaState["files"]["mkdir"] = [wrapper](const std::string &path) {
        return wrapper->mkdir(path);
    };
    extension->luaState["files"]["ls"] = [wrapper](const std::string &path, sol::this_state s) {
        return wrapper->ls(path, s);
    };

    sol::table mt = extension->luaState.create_table();
    mt["__index"] = [wrapper](sol::table t, std::string key, sol::this_state s) -> sol::object {
        if (key == "mainDirectory") {
            return wrapper->getMainDirectory(s);
        }
        return sol::lua_nil;
    };

    extension->luaState["files"][sol::metatable_key] = mt;
}
} // namespace extensions::files
