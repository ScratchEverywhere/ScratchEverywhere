#include "interpret.hpp"
#include "miniz/miniz.h"
#include "os.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#ifdef GAMECUBE
#include <dirent.h>
#include <gccore.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    static mz_zip_archive zipArchive;
    static std::vector<char> zipBuffer;
    static void *trackedBufferPtr;
    static size_t trackedBufferSize;
    static void *trackedJsonPtr;
    static size_t trackedJsonSize;

    static void openScratchProject(void *arg) {
        loadingState = "Opening Scratch project";
        Unzip::UnpackedInSD = false;
        std::ifstream file;
        int isFileOpen = openFile(&file);
        if (isFileOpen == 0) {
            Log::logError("Failed to open Scratch project.");
            Unzip::projectOpened = -1;
            Unzip::threadFinished = true;
            return;
        } else if (isFileOpen == -1) {
            Log::log("Main Menu activated.");
            Unzip::projectOpened = -3;
            Unzip::threadFinished = true;
            return;
        }
        loadingState = "Unzipping Scratch project";
        nlohmann::json project_json = unzipProject(&file);
        if (project_json.empty()) {
            Log::logError("Project.json is empty.");
            Unzip::projectOpened = -2;
            Unzip::threadFinished = true;
            return;
        }
        loadingState = "Loading Sprites";
        loadSprites(project_json);
        Unzip::projectOpened = 1;
        Unzip::threadFinished = true;
        return;
    }

#ifdef GAMECUBE
    // use libogc for gamecube because i guess it doesn't support std::filesystem
    static std::vector<std::string> getProjectFiles(const std::string &directory) {
        std::vector<std::string> projectFiles;
        DIR *dir = opendir(directory.c_str());
        if (dir == nullptr) {
            return projectFiles;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == ".sb3") {
                projectFiles.push_back(fileName);
            }
        }
        closedir(dir);
        return projectFiles;
    }
#else
    static std::vector<std::string> getProjectFiles(const std::string &directory) {
        std::vector<std::string> projectFiles;

        if (!std::filesystem::exists(directory)) {
            Log::logWarning("Directory does not exist! " + directory);
            try {
                std::filesystem::create_directory(directory);
            } catch (...) {
            }
            return projectFiles;
        }

        if (!std::filesystem::is_directory(directory)) {
            Log::logWarning("Path is not a directory! " + directory);
            return projectFiles;
        }

        try {
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".sb3") {
                    projectFiles.push_back(entry.path().filename().string());
                }
            }
        } catch (const std::filesystem::filesystem_error &e) {
            Log::logWarning(std::string("Failed to open directory: ") + e.what());
        }

        return projectFiles;
    }
#endif

    static std::string getSplashText() {
        std::string textPath = "gfx/menu/splashText.txt";

        textPath = OS::getRomFSLocation() + textPath;

        std::vector<std::string> splashLines;
        std::ifstream file(textPath);

        if (!file.is_open()) {
            return "Everywhere!"; // fallback text
        }

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) { // skip empty lines
                splashLines.push_back(line);
            }
        }
        file.close();

        if (splashLines.empty()) {
            return "Everywhere!"; // fallback if file is empty
        }

        // Initialize random number generator with current time
        static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<size_t> dist(0, splashLines.size() - 1);

        std::string splash = splashLines[dist(rng)];

        // Replace {PlatformName} with OS::getPlatform()
        const std::string platformName = "{PlatformName}";
        const std::string platform = OS::getPlatform();

        size_t pos = 0;
        while ((pos = splash.find(platformName, pos)) != std::string::npos) {
            splash.replace(pos, platformName.size(), platform);
            pos += platform.size(); // move past replacement
        }

        return splash;
    }

    static nlohmann::json unzipProject(std::ifstream *file) {
        nlohmann::json project_json;

        if (projectType != UNZIPPED) {
            // read the file
            Log::log("Reading SB3...");
            std::streamsize size = file->tellg();
            file->seekg(0, std::ios::beg);
            zipBuffer.resize(size);
            if (!file->read(zipBuffer.data(), size)) {
                return project_json;
            }

            // Use RAW allocation function and store both pointer and size
            trackedBufferSize = zipBuffer.size();
            trackedBufferPtr = MemoryTracker::allocate(trackedBufferSize);

            // open ZIP file
            Log::log("Opening SB3 file...");
            memset(&zipArchive, 0, sizeof(zipArchive));
            if (!mz_zip_reader_init_mem(&zipArchive, zipBuffer.data(), zipBuffer.size(), 0)) {
                return project_json;
            }

            // extract project.json
            Log::log("Extracting project.json...");
            int file_index = mz_zip_reader_locate_file(&zipArchive, "project.json", NULL, 0);
            if (file_index < 0) {
                return project_json;
            }

            size_t json_size;
            const char *json_data = static_cast<const char *>(mz_zip_reader_extract_to_heap(&zipArchive, file_index, &json_size, 0));

#ifdef ENABLE_CLOUDVARS
            projectJSON = std::string(json_data, json_size);
#endif

            // Parse JSON file
            Log::log("Parsing project.json...");
            // Use RAW allocation and store pointer + size
            trackedJsonSize = json_size;
            trackedJsonPtr = MemoryTracker::allocate(trackedJsonSize);

            project_json = nlohmann::json::parse(std::string(json_data, json_size));
            mz_free((void *)json_data);

            // FIXED: Use RAW deallocate function
            if (trackedJsonPtr) {
                MemoryTracker::deallocate(trackedJsonPtr, trackedJsonSize);
                trackedJsonPtr = nullptr;
                trackedJsonSize = 0;
            }

        } else {
            file->clear();
            file->seekg(0, std::ios::beg);

            // get file size
            file->seekg(0, std::ios::end);
            std::streamsize size = file->tellg();
            file->seekg(0, std::ios::beg);

            // put file into string
            std::string json_content;
            json_content.reserve(size);
            json_content.assign(std::istreambuf_iterator<char>(*file),
                                std::istreambuf_iterator<char>());

#ifdef ENABLE_CLOUDVARS
            projectJSON = json_content;
#endif

            project_json = nlohmann::json::parse(json_content);
        }
        return project_json;
    }

    static int openFile(std::ifstream *file);

    static bool load();

    static bool extractProject(const std::string &zipPath, const std::string &destFolder) {
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) {
            Log::logError("Failed to open zip: " + zipPath);
            return false;
        }

        int numFiles = (int)mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < numFiles; i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
            std::string filename(st.m_filename);

            if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
                continue;

#ifdef GAMECUBE
            mkdir(destFolder.c_str(), 0777);
#endif
            std::string outPath = destFolder + "/" + filename;

#ifndef GAMECUBE
            std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
#endif

            if (!mz_zip_reader_extract_to_file(&zip, i, outPath.c_str(), 0)) {
                Log::logError("Failed to extract: " + outPath);
                mz_zip_reader_end(&zip);
                return false;
            }
        }

        mz_zip_reader_end(&zip);
        return true;
    }

    static bool deleteProjectFolder(const std::string &directory) {
        if (!std::filesystem::exists(directory)) {
            Log::logWarning("Directory does not exist: " + directory);
            return false;
        }

        if (!std::filesystem::is_directory(directory)) {
            Log::logWarning("Path is not a directory: " + directory);
            return false;
        }

        try {
            std::filesystem::remove_all(directory);
            return true;
        } catch (const std::filesystem::filesystem_error &e) {
            Log::logError(std::string("Failed to delete folder: ") + e.what());
            return false;
        }
    }

    static nlohmann::json getSetting(const std::string &settingName) {
        std::string folderPath = OS::getScratchFolderLocation() + filePath + ".json";

        std::ifstream file(folderPath);
        if (!file.good()) {
            Log::logError("JSON file not found: " + folderPath);
            return nlohmann::json();
        }

        nlohmann::json json;
        try {
            file >> json;
        } catch (const nlohmann::json::parse_error &e) {
            Log::logError("Failed to parse JSON file: " + std::string(e.what()));
            file.close();
            return nlohmann::json();
        }
        file.close();

        if (!json.contains("settings")) {
            return nlohmann::json();
        }
        if (!json["settings"].contains(settingName)) {
            return nlohmann::json();
        }

        return json["settings"][settingName];
    }
};
