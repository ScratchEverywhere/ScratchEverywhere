#include "interpret.hpp"
#include "os.hpp"
#include <filesystem>
#include <fstream>
#ifdef GAMECUBE
#include <dirent.h>
#include <gccore.h>
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
    static mz_zip_archive zipArchive;
    static std::vector<char> zipBuffer;

    static void openScratchProject(void *arg) {
        loadingState = "Opening Scratch project";
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
    static std::vector<std::string> getProjectFiles(const std::string directory) {
        std::vector<std::string> projectFiles;
        DIR *dir = opendir(directory.c_str());
        if (dir == nullptr) {
            return projectFiles; // Return empty if directory can't be opened
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

#if defined(__WIIU__) || defined(__OGC__) || defined(__SWITCH__) || defined(__3DS__)
        textPath = "romfs:/" + textPath;
#endif

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
            std::streamsize size = file->tellg(); // gets the size of the file
            file->seekg(0, std::ios::beg);        // go to the beginning of the file
            zipBuffer.resize(size);
            if (!file->read(zipBuffer.data(), size)) {
                return project_json;
            }
            size_t bufferSize = zipBuffer.size();

            // Track memory
            MemoryTracker::allocate(bufferSize);

            // open ZIP file from the thing that we just did
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
            MemoryTracker::allocate(json_size);
            project_json = nlohmann::json::parse(std::string(json_data, json_size));
            mz_free((void *)json_data);
            MemoryTracker::deallocate(nullptr, json_size);

            // Image::loadImages(&zipArchive);
            // mz_zip_reader_end(&zipArchive);
        } else {
            // if project is unzipped
            file->clear();                 // Clear any EOF flags
            file->seekg(0, std::ios::beg); // Go to the start of the file
#ifdef ENABLE_CLOUDVARS
            projectJSON = {std::istreambuf_iterator<char>(*file), std::istreambuf_iterator<char>()};
#endif
            (*file) >> project_json;
        }
        Image::loadImages(&zipArchive);
        return project_json;
    }

    static int openFile(std::ifstream *file);

    static bool load();
};
