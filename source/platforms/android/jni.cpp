#include <jni.h>
#include "os.hpp"

extern "C"
JNIEXPORT jstring JNICALL
Java_io_github_scratcheverywhere_ProjectImportActivity_getScratchFolder(JNIEnv *env, jclass) {
    const std::string path = OS::getScratchFolderLocation();
    return env->NewStringUTF(path.c_str());
}