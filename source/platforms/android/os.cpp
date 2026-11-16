#include <log.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

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
#else
    return false;
#endif
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

extern "C" JNIEXPORT void JNICALL
Java_io_github_scratcheverywhere_MainActivity_loadProject(
    JNIEnv *env, jobject thiz, jstring path) {
    const char *project = env->GetStringUTFChars(path, JNI_FALSE);

    // When a project is imported from a "Open with" dialog, we will start the project
    Unzip::filePath = project;
    Scratch::nextProject = true;
    if (Scratch::proje)
    Scratch::shouldStop = true;

    env->ReleaseStringUTFChars(path, project);
}
