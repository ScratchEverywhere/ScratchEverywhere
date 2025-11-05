#pragma once
#include <cctype>
#ifdef __3DS__
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#endif
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __3DS__
#include <3ds.h>
#else
#include <mutex>
#include <thread>
#endif

struct DownloadItem {
    std::string url;
    std::string filepath;
    bool finished = false;
    bool success = false;
    std::string error;
};

struct FileWriteData {
    std::ofstream *file;
    size_t bytesWritten = 0;
};

static inline std::string urlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else
            escaped << '%' << std::uppercase << std::setw(2) << int(c) << std::nouppercase;
    }
    return escaped.str();
}

#if !defined(ENABLE_CLOUDVARS) || (!defined(__WIIU__) && !defined(__3DS__) && !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__))
    class DownloadManager {
    public:
    static void init() {return;};
    static void deinit() {return;};
    static void addDownload(const std::string &url, const std::string &filepath) {return;};
    static bool isDownloading(const std::string &url) {return false;};
    static std::shared_ptr<DownloadItem> getDownloaded(const std::string &url) {return nullptr;};
    static void removeFromMemory(const std::string &url) {return;};
    static void join() {return;};
    static void processQueueThreaded() {return;};
    };
#else
class DownloadManager {
  public:
    static void init();
    static void deinit();
    static void addDownload(const std::string &url, const std::string &filepath);
    static bool isDownloading(const std::string &url);
    static std::shared_ptr<DownloadItem> getDownloaded(const std::string &url);
    static void removeFromMemory(const std::string &url);
    static void join();
    static void processQueueThreaded();

  private:
    static void startThread();
    static void performDownload(std::shared_ptr<DownloadItem> item);
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    static inline std::vector<std::shared_ptr<DownloadItem>> pendingDownloads;
    static inline std::unordered_map<std::string, std::shared_ptr<DownloadItem>> pendingMap;
    static inline std::unordered_map<std::string, std::shared_ptr<DownloadItem>> downloadedMap;

#ifdef __3DS__
    static inline Thread downloadThread;
    static LightLock mtx;
#else
    static inline std::mutex mtx;
    static inline std::thread downloadThread;
#endif
};
#endif
