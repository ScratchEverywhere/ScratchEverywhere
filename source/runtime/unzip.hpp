#pragma once

#include "interpret.hpp"
#include <cstring>
#include <errno.h>
#include <fstream>
#include <miniz.h>
#include <os.hpp>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
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

    static void openScratchProject(void *arg) {
        loadingState = "Opening Scratch project";
        Unzip::UnpackedInSD = false;
        std::istream *file = nullptr;

        int isFileOpen = openFile(file);
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
        nlohmann::json project_json = unzipProject(file);
        if (project_json.empty()) {
            Log::logError("Project.json is empty.");
            Unzip::projectOpened = -2;
            Unzip::threadFinished = true;
            delete file;
            return;
        }
        loadingState = "Loading Sprites";
        loadSprites(project_json);
        Unzip::projectOpened = 1;
        Unzip::threadFinished = true;
        delete file;
        return;
    }

    static std::vector<std::string> getProjectFiles(const std::string &directory) {
        std::vector<std::string> projectFiles;
        struct stat dirStat;

        if (stat(directory.c_str(), &dirStat) != 0) {
            Log::logWarning("Directory does not exist! " + directory);
            try {
                OS::createDirectory(directory);
            } catch (...) {
            }
            return projectFiles;
        }

        if (!(dirStat.st_mode & S_IFDIR)) {
            Log::logWarning("Path is not a directory! " + directory);
            return projectFiles;
        }

#ifdef _WIN32
        std::wstring wdirectory(directory.size(), L' ');
        wdirectory.resize(std::mbstowcs(&wdirectory[0], directory.c_str(), directory.size()));
        if (wdirectory.back() != L'\\') wdirectory += L"\\";
        wdirectory += L"*";

        WIN32_FIND_DATAW find_data;
        HANDLE hfind = FindFirstFileW(wdirectory.c_str(), &find_data);

        do {
            std::wstring wname(find_data.cFileName);

            if (wcscmp(wname.c_str(), L".") == 0 || wcscmp(wname.c_str(), L"..") == 0)
                continue;

            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const wchar_t *ext = wcsrchr(wname.c_str(), L'.');
                if (ext && _wcsicmp(ext, L".sb3") == 0) {
                    std::string name(wname.size(), ' ');
                    name.resize(std::wcstombs(&name[0], wname.c_str(), wname.size()));
                    projectFiles.push_back(name);
                }
            }
        } while (FindNextFileW(hfind, &find_data));

        FindClose(hfind);
#else
        DIR *dir = opendir(directory.c_str());
        if (!dir) {
            Log::logWarning("Failed to open directory: " + std::string(strerror(errno)));
            return projectFiles;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            std::string fullPath = directory + entry->d_name;

            struct stat fileStat;
            if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && strcmp(ext, ".sb3") == 0) {
                    projectFiles.push_back(entry->d_name);
                }
            }
        }

        closedir(dir);
#endif
        return projectFiles;
    }

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

    static nlohmann::json unzipProject(std::istream *file) {
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

            project_json = nlohmann::json::parse(std::string(json_data, json_size));
            mz_free((void *)json_data);

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

    static int openFile(std::istream *&file);

    static bool load();

    static bool extractProject(const std::string &zipPath, const std::string &destFolder) {
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) {
            Log::logError("Failed to open zip: " + zipPath);
            return false;
        }

        try {

            OS::createDirectory(destFolder + "/");
        } catch (const std::exception &e) {
            Log::logError(e.what());
            return false;
        }

        int numFiles = (int)mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < numFiles; i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
            std::string filename(st.m_filename);

            if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
                continue;

            std::string outPath = destFolder + "/" + filename;

            OS::createDirectory(OS::parentPath(outPath));

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
        struct stat st;
        if (stat(directory.c_str(), &st) != 0) {
            Log::logWarning("Directory does not exist: " + directory);
            return false;
        }

        if (!(st.st_mode & S_IFDIR)) {
            Log::logWarning("Path is not a directory: " + directory);
            return false;
        }

        try {
            OS::removeDirectory(directory);
            return true;
        } catch (const OS::FilesystemError &e) {
            Log::logError(std::string("Failed to delete folder: ") + e.what());
        }

        return true;
    }

    static nlohmann::json getSetting(const std::string &settingName) {
        std::string folderPath = filePath + ".json";

        std::ifstream file(folderPath);
        if (!file.good()) {
            Log::logWarning("Project settings file not found: " + folderPath);
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
