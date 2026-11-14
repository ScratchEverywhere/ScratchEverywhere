#ifdef ENABLE_DOWNLOAD
#include "downloader.hpp"
#ifdef __3DS__
#include <3ds.h>
#endif
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <net/http.hpp>
#include <socket.hpp>
#include <sys/stat.h>

static bool workerRunning = false;

static void N3DS_ProcessQueueThreadedWrapper(void *arg) {
    DownloadManager::processQueueThreaded();
}

bool DownloadManager::init() {
    if (isInitialized) return true;
    if (!SE_Socket::init()) return false;
    mtx.init();
    isInitialized = true;
    return true;
}

void DownloadManager::deinit() {
    if (!isInitialized) return;
    join();
    SE_Socket::deinit();
    isInitialized = false;
}

void DownloadManager::startThread() {
    mtx.lock();
    if (workerRunning) {
        mtx.unlock();
        return;
    }
    workerRunning = true;
    mtx.unlock();

    downloadThread.join();
    if (!downloadThread.create(N3DS_ProcessQueueThreadedWrapper, nullptr, 64 * 1024, 999, -2, "downloadThread")) {
        mtx.lock();
        workerRunning = false;
        mtx.unlock();
    }
}

void DownloadManager::join() {
    downloadThread.join();
}

void DownloadManager::addDownload(const std::string &url, const std::string &filepath) {
    mtx.lock();
    if (pendingMap.find(url) != pendingMap.end()) {
        mtx.unlock();
        return;
    }

    if (downloadedMap.find(url) != downloadedMap.end()) {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0) {
            mtx.unlock();
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

    mtx.unlock();
    startThread();
}

bool DownloadManager::isDownloading(const std::string &url) {
    mtx.lock();
    bool result = pendingMap.find(url) != pendingMap.end();
    mtx.unlock();
    return result;
}

std::shared_ptr<DownloadItem> DownloadManager::getDownloaded(const std::string &url) {
    mtx.lock();
    auto it = downloadedMap.find(url);
    auto result = (it != downloadedMap.end()) ? it->second : nullptr;
    mtx.unlock();
    return result;
}

void DownloadManager::removeFromMemory(const std::string &url) {
    mtx.lock();
    downloadedMap.erase(url);
    mtx.unlock();
}

void DownloadManager::processQueueThreaded() {
    while (true) {
        std::shared_ptr<DownloadItem> item = nullptr;

        mtx.lock();
        if (pendingDownloads.empty()) {
            workerRunning = false;
            mtx.unlock();
            break;
        }
        item = pendingDownloads.front();
        pendingDownloads.erase(pendingDownloads.begin());
        mtx.unlock();

        performDownload(item);
    }
    Log::log("DownloadManager: worker thread finished (queue empty)");
}

void DownloadManager::performDownload(std::shared_ptr<DownloadItem> item) {
    auto finish = [&](bool success, const std::string &error) {
        item->success = success;
        item->error = error;
        item->finished = true;

        mtx.lock();
        downloadedMap[item->url] = item;
        pendingMap.erase(item->url);
        mtx.unlock();
    };

    size_t lastSlash = item->filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        const std::string dir = item->filepath.substr(0, lastSlash) + "/";
        const auto err = FileSystem::createDirectory(dir.c_str());
        if (!err.has_value()) {
            Log::logWarning("Download failed: Could not create directory: " + err.error());
            finish(false, "Failed to create directory");
            return;
        }
    }

    std::ofstream outFile(item->filepath, std::ios::binary);
    if (!outFile.is_open()) {
        Log::log("DownloadManager: ERROR - cannot open output file: " + item->filepath);
        finish(false, "Failed to open file for writing");
        return;
    }

    HttpResponse response;
    bool ok = Http::get(
        item->url, [&outFile](const uint8_t *data, size_t len) {
            outFile.write(reinterpret_cast<const char *>(data), (std::streamsize)len);
            return outFile.good();
        },
        response);
    outFile.close();

    if (!ok) {
        const std::string error = response.error.empty() ? "Download failed" : response.error;
        Log::log("DownloadManager: ERROR - " + error);
        std::remove(item->filepath.c_str());
        finish(false, error);
        return;
    }

    Log::log("DownloadManager: download complete (" + item->filepath + ")");
    finish(true, "");
}

#endif
