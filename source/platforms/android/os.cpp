#include <log.hpp>
#include <os.hpp>
#include <filesystem.hpp>
#include <popupMenu.hpp>
#include <translation.hpp>

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

std::string OS::importProject(const std::string &sourcePath) {
    // Getting the filename is too complicated via C++, letting Java do it instead
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetJNIEnv());
    jclass clazz = env->GetObjectClass(activity);

    jmethodID getFilename = env->GetMethodID(clazz,
         "getFilename", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring inputParam = env->NewStringUTF(sourcePath.c_str());
    jstring filenameResult = static_cast<jstring>(
        env->CallObjectMethod(activity, getFilename, inputParam));

    const char *filename = env->GetStringUTFChars(filenameResult, JNI_FALSE);
    std::string projectPath = OS::getScratchFolderLocation() + filename;
    if (FileSystem::fileExists(projectPath)) {
        PopupMenu *popup = new PopupMenu(PopupType::ACCEPT_OR_CANCEL,
                                         TranslationManager::getTranslation("ui.popup.importOverwrite"));
        MenuManager::changeMenu(popup);
        while (Render::appShouldRun() && popup->accepted == -1) {
            MenuManager::render();
        }
        popup->cleanup();

        if (popup->accepted != 0)
            return projectPath; // Already in the filesystem
    }

    jmethodID importMethod = env->GetMethodID(clazz,
         "importProject", "(Ljava/lang/String;Ljava/lang/String;)V");
    jstring outputParam = env->NewStringUTF(projectPath.c_str());
    jboolean result = env->CallBooleanMethod(
        activity, importMethod, inputParam, outputParam);

    env->ReleaseStringUTFChars(filenameResult, filename);
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(activity);
    return result == JNI_TRUE ? projectPath : "";
}

bool OS::isOnline() {
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    // Call Java function
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    jclass clazz = env->GetObjectClass(activity);
    jmethodID isOnline = env->GetMethodID(clazz, "isOnline", "()Z");
    jboolean result = env->CallBooleanMethod(activity, isOnline);
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(activity);
    return result == JNI_TRUE;
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
