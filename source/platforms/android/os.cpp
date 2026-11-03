#include <fstream>
#include <log.hpp>
#include <os.hpp>

#include <jni.h>
#include <SDL_system.h>
#include <SDL_timer.h>

#ifndef USE_BUNDLE
#include <android/asset_manager_jni.h>
AAssetManager *g_assetManager = nullptr;
#endif

#include <loading.hpp>
#include <mainMenu.hpp>
#include <thread.hpp>
#include <translation.hpp>

// Import threads
struct ImportThreadArgs {
    int fd;
    std::string filename;
};

static void importProject(void *arg) {
    ImportThreadArgs *args = static_cast<ImportThreadArgs *>(arg);
    std::string destPath = OS::getScratchFolderLocation() + args->filename;

    FILE *file = fdopen(args->fd, "rb");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::vector<char> buffer(size);
    if (fread(buffer.data(), 1, size, file) != size) {
        fclose(file);
        Log::logError("Failed to read project: " + args->filename);
        return;
    }
    fclose(file);

    std::ofstream dest(destPath, std::ios::binary | std::ios::trunc);
    dest.write(buffer.data(), buffer.size());
    dest.close();

    Unzip::filePath = destPath;
    MenuManager::loadProject();
}

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
    JNIEnv *env, jobject thiz, jint fd, jstring filename) {
    const char *jniFilename = env->GetStringUTFChars(filename, JNI_FALSE);
    const std::string cppFilename(jniFilename);
    env->ReleaseStringUTFChars(filename, jniFilename);

    // Advice user we are yielding until runtime is initialized using a Toast
    if (!Unzip::projectOpened) {
        TranslationManager::loadLanguage();
        jclass clazz = env->FindClass("android/widget/Toast");
        jmethodID makeText = env->GetStaticMethodID(
            clazz, "makeText",
            "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;"
        );
        jmethodID show = env->GetMethodID(clazz, "show", "()V");
        jstring message = env->NewStringUTF(TranslationManager::getTranslation("ui.unpack.wait").c_str());
        jobject toast = env->CallStaticObjectMethod(clazz, makeText, thiz, message, 0);
        env->CallVoidMethod(toast, show);
        env->DeleteLocalRef(toast);
        env->DeleteLocalRef(clazz);
    }

    nlohmann::json json = SettingsManager::getConfigSettings();
    bool shouldImport = true;

    if (json.contains("ImportSettings") && json["ImportSettings"].is_boolean()) {
        shouldImport = json["ImportSettings"].get<bool>();;
    }

    if (shouldImport) {
        ImportThreadArgs args{fd, cppFilename};
#if defined(ENABLE_LOADSCREEN) && defined(ENABLE_MENU)
        SE_Thread importThread;
        if (importThread.create(importProject, &args, 0x262144, 0, -1, "ProjectImporter")) {
            Loading loading;
            loading.init();

            Unzip::loadingState = TranslationManager::getTranslation("ui.loading.import");
            while (!Unzip::threadFinished) {
                loading.render();
            }

            loading.cleanup();
            importThread.join();
        }
#else
        // Non-threaded loading fallback
        importProject(&args);
#endif
    }


}
