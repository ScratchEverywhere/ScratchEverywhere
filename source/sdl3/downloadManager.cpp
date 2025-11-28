#ifdef ENABLE_DOWNLOAD
#include "downloader.hpp"
#include "os.hpp"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <atomic>

static std::atomic<bool> workerRunning(false);

void DownloadManager::init() {
    Log::log("DownloadManager: init()");
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK)
        Log::log(std::string("curl_global_init failed: ") + curl_easy_strerror(res));
}

void DownloadManager::deinit() {
    join();
    curl_global_cleanup();
}

void DownloadManager::startThread() {
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
}

void DownloadManager::join() {
    if (downloadThread.joinable()) {
        downloadThread.join();
        downloadThread = std::thread();
    }
}

void DownloadManager::addDownload(const std::string &url, const std::string &filepath) {
    std::lock_guard<std::mutex> lock(mtx);

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
    pendingDownloads.push_back(item);
    pendingMap[url] = item;

    Log::log("DownloadManager: added download to queue (" + url + ")");
    startThread();
}

bool DownloadManager::isDownloading(const std::string &url) {
    std::lock_guard<std::mutex> lock(mtx);
    return pendingMap.find(url) != pendingMap.end();
}

std::shared_ptr<DownloadItem> DownloadManager::getDownloaded(const std::string &url) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = downloadedMap.find(url);
    return (it != downloadedMap.end()) ? it->second : nullptr;
}

void DownloadManager::removeFromMemory(const std::string &url) {
    std::lock_guard<std::mutex> lock(mtx);
    downloadedMap.erase(url);
}

void DownloadManager::processQueueThreaded() {
    workerRunning.store(true);
    while (true) {
        std::shared_ptr<DownloadItem> item = nullptr;

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (pendingDownloads.empty()) break;
            item = pendingDownloads.front();
            pendingDownloads.erase(pendingDownloads.begin());
        }

        performDownload(item);
    }
    Log::log("DownloadManager: worker thread finished (queue empty)");
    workerRunning.store(false);
}

void DownloadManager::performDownload(std::shared_ptr<DownloadItem> item) {

    CURL *curl = curl_easy_init();
    if (!curl) {
        item->finished = true;
        item->success = false;
        item->error = "curl init failed";
        Log::log("DownloadManager: ERROR - curl init failed");
        return;
    }

    std::string path = item->filepath;
    auto pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0777);
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
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Downloader/1.0");

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

    item->finished = true;

    {
        std::lock_guard<std::mutex> lock(mtx);
        downloadedMap[item->url] = item;
        pendingMap.erase(item->url);
    }
    curl_easy_reset(curl);
    curl_easy_cleanup(curl);
}

size_t DownloadManager::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    FileWriteData *data = static_cast<FileWriteData *>(userp);
    data->file->write(static_cast<const char *>(contents), totalSize);
    data->bytesWritten += totalSize;
    return totalSize;
}

#endif