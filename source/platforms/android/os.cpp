#include <log.hpp>
#include <os.hpp>

#include <jni.h>
#include <SDL_system.h>

#ifndef USE_BUNDLE
#include <android/asset_manager_jni.h>
AAssetManager *g_assetManager = nullptr;
#endif


namespace OS {
    bool toExit = false;
    bool loadedSettings = false;
    std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "Android";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "/storage/emulated/0/";
}

std::string OS::getConfigFolderLocation() {
    return std::string(SDL_AndroidGetExternalStoragePath()) + "/";
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;

    return std::string(getConfigFolderLocation());
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    return "Player";
}

// JNI functions
extern "C" JNIEXPORT void JNICALL
Java_io_github_scratcheverywhere_MainActivity_initAssets(
    JNIEnv* env, jobject thiz, jobject manager) {
#ifndef USE_BUNDLE
    g_assetManager = AAssetManager_fromJava(env, manager);
    if (g_assetManager == nullptr)
        Log::logCritical("Failed to initialize Android AAsset manager", true);
#endif
}
