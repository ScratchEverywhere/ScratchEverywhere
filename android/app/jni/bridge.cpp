#include <android/log.h>
#include <iostream>
#include <jni.h>
#define LOGCAT_TAG "ScratchEverywhere"

// std::cout -> Logcat logger
class LogcatBuffer : public std::streambuf {
  protected:
    android_LogPriority priority_;
    const char *tag_;
    std::string buffer_;
    void flush() {
        if (!buffer_.empty()) {
            __android_log_print(priority_, tag_, "%s", buffer_.c_str());
            buffer_.clear();
        }
    }
    int overflow(int c) override {
        if (c == '\n') {
            flush();
        } else if (c != EOF) {
            buffer_ += static_cast<char>(c);
        }
        return c;
    }
    int sync() override {
        flush();
        return 0;
    }

  public:
    LogcatBuffer(android_LogPriority priority, const char *tag) : priority_(priority), tag_(tag) {}
};

static LogcatBuffer logcat_out_buffer(ANDROID_LOG_INFO, LOGCAT_TAG);
static LogcatBuffer logcat_err_buffer(ANDROID_LOG_ERROR, LOGCAT_TAG);
extern "C" JNIEXPORT jint JNICALL Java_io_github_scratcheverywhere_MainActivity_nativeSetupJNI(JNIEnv *, jclass) {
    std::cout.rdbuf(&logcat_out_buffer);
    std::cerr.rdbuf(&logcat_err_buffer);
    return 0;
}