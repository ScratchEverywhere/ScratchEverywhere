#ifdef ENABLE_DOWNLOAD
#include "downloader.hpp"
#ifdef __3DS__
#include <3ds.h>
#endif
#include "os.hpp"
#include <atomic>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>

#ifdef __3DS__
LightLock DownloadManager::mtx;
static LightLock workerLock;
static bool workerRunning = false;
#else
static std::atomic<bool> workerRunning(false);
#endif

static void N3DS_ProcessQueueThreadedWrapper(void *arg) {
    DownloadManager::processQueueThreaded();
}

bool DownloadManager::init() {
    if (isInitialized) return true;
    if (!OS::initWifi()) return false;
#ifdef __3DS__
    LightLock_Init(&mtx);
#endif
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        Log::log(std::string("curl_global_init failed: ") + curl_easy_strerror(res));
        return false;
    }
#ifdef __3DS__
    LightLock_Init(&workerLock);
#endif
    isInitialized = true;
    return true;
}

void DownloadManager::deinit() {
    if (!isInitialized) return;
    join();
    curl_global_cleanup();
    OS::deInitWifi();
    isInitialized = false;
}

void DownloadManager::startThread() {
#ifdef __3DS__
    if (downloadThread) {
        bool isRunning;
        LightLock_Lock(&workerLock);
        isRunning = workerRunning;
        LightLock_Unlock(&workerLock);
        if (!isRunning) {
            threadJoin(downloadThread, 0);
            threadFree(downloadThread);
            downloadThread = nullptr;
        } else {

            return;
        }
    }
    downloadThread = threadCreate(N3DS_ProcessQueueThreadedWrapper, nullptr, 8 * 1024, 0x3F, -2, true);
#else
    std::lock_guard<std::mutex> lock(mtx);
    if (downloadThread.joinable()) {
        if (!workerRunning.load()) {
            downloadThread.join();
            downloadThread = std::thread();
        } else {
            return;
        }
    }
    downloadThread = std::thread(&DownloadManager::processQueueThreaded);
#endif
}

void DownloadManager::join() {
#ifdef __3DS__
    if (downloadThread) {
        threadJoin(downloadThread, U64_MAX);
        threadFree(downloadThread);
        downloadThread = nullptr;
    }
#else
    if (downloadThread.joinable()) {
        downloadThread.join();
        downloadThread = std::thread();
    }
#endif
}

void DownloadManager::addDownload(const std::string &url, const std::string &filepath) {
#ifdef __3DS__
    LightLock_Lock(&mtx);
#else
    std::lock_guard<std::mutex> lock(mtx);
#endif

    if (pendingMap.find(url) != pendingMap.end()) {
        return;
    }

    if (downloadedMap.find(url) != downloadedMap.end()) {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0) {
            return;
        } else {
            downloadedMap.erase(url);
        }
    }

    auto item = std::make_shared<DownloadItem>();
    item->url = url;
    item->filepath = filepath;
    item->finished = false;
    item->success = false;
    pendingDownloads.push_back(item);
    pendingMap[url] = item;

#ifdef __3DS__
    LightLock_Unlock(&mtx);
#endif
    startThread();
}

bool DownloadManager::isDownloading(const std::string &url) {
#ifdef __3DS__
    LightLock_Lock(&mtx);
    bool result = pendingMap.find(url) != pendingMap.end();
    LightLock_Unlock(&mtx);
    return result;
#else
    std::lock_guard<std::mutex> lock(mtx);
    return pendingMap.find(url) != pendingMap.end();
#endif
}

std::shared_ptr<DownloadItem> DownloadManager::getDownloaded(const std::string &url) {
#ifdef __3DS__
    LightLock_Lock(&mtx);
#else
    std::lock_guard<std::mutex> lock(mtx);
#endif
    auto it = downloadedMap.find(url);
    auto result = (it != downloadedMap.end()) ? it->second : nullptr;
#ifdef __3DS__
    LightLock_Unlock(&mtx);
#endif
    return result;
}

void DownloadManager::removeFromMemory(const std::string &url) {
#ifdef __3DS__
    LightLock_Lock(&mtx);
#else
    std::lock_guard<std::mutex> lock(mtx);
#endif
    downloadedMap.erase(url);
#ifdef __3DS__
    LightLock_Unlock(&mtx);
#endif
}

void DownloadManager::processQueueThreaded() {
#ifdef __3DS__
    LightLock_Lock(&mtx);
    workerRunning = true;
    LightLock_Unlock(&mtx);
#else
    workerRunning.store(true);
#endif
    while (true) {
        std::shared_ptr<DownloadItem> item = nullptr;

        {
#ifdef __3DS__
            LightLock_Lock(&mtx);
#else
            std::lock_guard<std::mutex> lock(mtx);
#endif
            if (pendingDownloads.empty()) {
#ifdef __3DS__
                LightLock_Unlock(&mtx);
#endif
                break;
            }
            item = pendingDownloads.front();
            pendingDownloads.erase(pendingDownloads.begin());
#ifdef __3DS__
            LightLock_Unlock(&mtx);
#endif
        }

        performDownload(item);
    }
    // Log::log("DownloadManager: worker thread finished (queue empty)");
#ifdef __3DS__
    LightLock_Lock(&mtx);
    workerRunning = false;
    LightLock_Unlock(&mtx);
#else
    workerRunning.store(false);
#endif
}

void DownloadManager::performDownload(std::shared_ptr<DownloadItem> item) {

    CURL *curl = curl_easy_init();
    if (!curl) {
        item->finished = true;
        item->success = false;
        item->error = "curl init failed";
        Log::logError("DownloadManager: curl init failed");
        return;
    }

    size_t lastSlash = item->filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string dir = item->filepath.substr(0, lastSlash);
        if (mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) {
            item->finished = true;
            item->success = false;
            item->error = "Failed to create directory";
            Log::log("Download failed: Could not create directory");
            curl_easy_cleanup(curl);
            return;
        }
    }

    std::ofstream outFile(item->filepath, std::ios::binary);
    if (!outFile.is_open()) {
        item->finished = true;
        item->success = false;
        item->error = "Failed to open file for writing";
        Log::log("DownloadManager: ERROR - cannot open output file: " + item->filepath);
        curl_easy_cleanup(curl);
        return;
    }

    FileWriteData writeData{&outFile, 0};
    curl_easy_setopt(curl, CURLOPT_URL, item->url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SE-Downloader/1.0");

    CURLcode res = curl_easy_perform(curl);
    outFile.close();

    item->success = (res == CURLE_OK);
    if (!item->success) {
        item->error = curl_easy_strerror(res);
        Log::log("DownloadManager: ERROR - " + std::string(curl_easy_strerror(res)));
        remove(item->filepath.c_str());
    } else {
        Log::log("DownloadManager: download complete (" + item->filepath + ")");
    }

    curl_easy_cleanup(curl);
    curl_easy_reset(curl);

    item->finished = true;

    {
#ifdef __3DS__
        LightLock_Lock(&mtx);
#else
        std::lock_guard<std::mutex> lock(mtx);
#endif
        downloadedMap[item->url] = item;
        pendingMap.erase(item->url);
#ifdef __3DS__
        LightLock_Unlock(&mtx);
#endif
    }
}

size_t DownloadManager::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    FileWriteData *data = static_cast<FileWriteData *>(userp);
    data->file->write(static_cast<const char *>(contents), totalSize);
    data->bytesWritten += totalSize;
    return totalSize;
}

#endif