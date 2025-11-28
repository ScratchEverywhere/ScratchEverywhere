#ifdef ENABLE_DOWNLOAD
#include "downloader.hpp"
#include "os.hpp"
#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

LightLock DownloadManager::mtx;

static bool workerRunning = false;
static LightLock workerLock;

static void processQueueThreadedWrapper(void *arg) {
    DownloadManager::processQueueThreaded();
}

void DownloadManager::init() {
    LightLock_Init(&mtx);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    LightLock_Init(&workerLock);
}

void DownloadManager::deinit() {
    join();
    curl_global_cleanup();
}

void DownloadManager::startThread() {

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
    downloadThread = threadCreate(processQueueThreadedWrapper, nullptr, 8 * 1024, 0x3F, -2, true);
}

void DownloadManager::join() {
    if (downloadThread) {
        threadJoin(downloadThread, U64_MAX);
        threadFree(downloadThread);
        downloadThread = nullptr;
    }
}

void DownloadManager::addDownload(const std::string &url, const std::string &filepath) {
    LightLock_Lock(&mtx);

    auto item = std::make_shared<DownloadItem>();
    item->url = url;
    item->filepath = filepath;
    item->finished = false;
    item->success = false;
    pendingDownloads.push_back(item);
    pendingMap[url] = item;

    LightLock_Unlock(&mtx);
    startThread();
}

bool DownloadManager::isDownloading(const std::string &url) {
    LightLock_Lock(&mtx);
    bool result = pendingMap.find(url) != pendingMap.end();
    LightLock_Unlock(&mtx);
    return result;
}

std::shared_ptr<DownloadItem> DownloadManager::getDownloaded(const std::string &url) {
    LightLock_Lock(&mtx);
    auto it = downloadedMap.find(url);
    auto result = (it != downloadedMap.end()) ? it->second : nullptr;
    LightLock_Unlock(&mtx);
    return result;
}

void DownloadManager::removeFromMemory(const std::string &url) {
    LightLock_Lock(&mtx);
    downloadedMap.erase(url);
    LightLock_Unlock(&mtx);
}

void DownloadManager::processQueueThreaded() {
    LightLock_Lock(&workerLock);
    workerRunning = true;
    LightLock_Unlock(&workerLock);
    while (true) {
        std::shared_ptr<DownloadItem> item = nullptr;

        LightLock_Lock(&mtx);
        if (pendingDownloads.empty()) {
            LightLock_Unlock(&mtx);
            break;
        }
        item = pendingDownloads.front();
        pendingDownloads.erase(pendingDownloads.begin());
        LightLock_Unlock(&mtx);

        performDownload(item);
    }
    LightLock_Lock(&workerLock);
    workerRunning = false;
    LightLock_Unlock(&workerLock);
}

void DownloadManager::performDownload(std::shared_ptr<DownloadItem> item) {
    /*if (!std::filesystem::exists("romfs:/gfx/certs.pem")) {
        Log::log("DownloadManager: certs.pem not found, download cannot proceed.");
        item->finished = true;
        item->success = false;
        item->error = "certs.pem not found";
        return;
    }*/

    CURL *curl = curl_easy_init();
    if (!curl) {
        item->finished = true;
        item->success = false;
        item->error = "Failed to init curl";
        Log::log("Download failed: CURL init error");
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
        item->error = "Failed to open output file";
        Log::log("Download failed: Could not open output file: " + item->filepath);
        curl_easy_cleanup(curl);
        return;
    }

    FileWriteData writeData{&outFile, 0};

    curl_easy_setopt(curl, CURLOPT_URL, item->url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
    // curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/gfx/certs.pem");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "3DS-Downloader/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    outFile.close();

    item->success = (res == CURLE_OK);
    if (!item->success) {
        item->error = curl_easy_strerror(res);
        Log::log("Download failed: " + std::string(item->error));
        remove(item->filepath.c_str());
    }

    curl_easy_cleanup(curl);

    item->finished = true;

    LightLock_Lock(&mtx);
    if (item->success) {
        downloadedMap[item->url] = item;
    }
    pendingMap.erase(item->url);
    LightLock_Unlock(&mtx);
}

size_t DownloadManager::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    FileWriteData *writeData = static_cast<FileWriteData *>(userp);
    writeData->file->write(static_cast<char *>(contents), totalSize);
    writeData->bytesWritten += totalSize;
    return totalSize;
}
#endif