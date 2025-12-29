#include "unzip.hpp"
#include "input.hpp"
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fstream>
#include <image.hpp>
#include <istream>
#include <menus/loading.hpp>
#include <os.hpp>
#include <random>
#include <runtime.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef __3DS__
#include <3ds.h>
#elif defined(RENDERER_SDL1)
#include "SDL/SDL.h"
#elif defined(RENDERER_SDL2)
#include "SDL2/SDL.h"
#elif defined(RENDERER_SDL3)
#include "SDL3/SDL.h"
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
#include <sstream>

CMRC_DECLARE(romfs);
#endif

int projectLoaderThread(void *data) {
    Unzip::openScratchProject(NULL);
    return 0;
}

bool Unzip::load() {

    Unzip::threadFinished = false;
    Unzip::error = "";

#ifdef ENABLE_LOADSCREEN

#ifdef __3DS__ // create 3DS thread for loading screen
    s32 mainPrio = 0;
    svcGetThreadPriority(&mainPrio, CUR_THREAD_HANDLE);

    Thread projectThread = threadCreate(
        Unzip::openScratchProject,
        NULL,
        0x4000,
        mainPrio + 1,
        -1,
        false);

    if (!projectThread) {
        if (Unzip::error.empty()) error = "The thread for reading the game could not be started.";
        Unzip::threadFinished = true;
    }

    Loading loading;
    loading.init();

    while (!Unzip::threadFinished) {
        loading.render();
    }
    threadJoin(projectThread, U64_MAX);
    threadFree(projectThread);
    if (!Unzip::error.empty()) { // if error exist go back to Main Menu
        loading.cleanup();
        return false;
    }
    loading.cleanup();
    osSetSpeedupEnable(false);

#elif defined(RENDERER_SDL1) | defined(RENDERER_SDL2) || defined(RENDERER_SDL3) // create SDL thread for loading screen
#ifdef RENDERER_SDL1
    SDL_Thread *thread = SDL_CreateThread(projectLoaderThread, nullptr);
#elif defined(RENDERER_SDL2)
    SDL_Thread *thread = SDL_CreateThreadWithStackSize(projectLoaderThread, "LoadingScreen", 0x15000, nullptr);
#else
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)projectLoaderThread);
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "LoadingScreen");
    SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 0x15000);
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, nullptr);
    SDL_Thread *thread = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
#endif

    if (thread != NULL && thread != nullptr) {

        Loading loading;
        loading.init();

        while (!Unzip::threadFinished) {
            loading.render();
        }
        SDL_WaitThread(thread, nullptr);
        loading.cleanup();
    } else Unzip::openScratchProject(NULL);

    if (!Unzip::error.empty()) // if error exist go back to Main Menu
        return false;
#endif
#else

    // non-threaded loading
    Unzip::openScratchProject(NULL);
    if (!error.empty())
        return false;
#endif
    return true;
}

void Unzip::openScratchProject(void *arg) {
    loadingState = "Opening Scratch project";
    error = "";
    // detect if embedded File:
    std::string embeddedFilename = OS::getRomFSLocation() + "project.sb3";
    std::string unzippedPath = OS::getRomFSLocation() + "project/";
    Scratch::projectLocation = Scratch::ProjectLocation::EMBEDDED;
    if (OS::fileExists(embeddedFilename)) {
        Scratch::projectType = Scratch::ProjectType::ARCHIVE;
        filePath = embeddedFilename;
    } else if (OS::fileExists(unzippedPath)) {
        Scratch::projectType = Scratch::ProjectType::EXTRACTED;
        filePath = unzippedPath;
    }

    if (filePath.empty()) return;

    Scratch::projectLocation = Scratch::ProjectLocation::UNEMBEDDED;

    if (OS::fileExists(filePath)) Scratch::projectType = Scratch::ProjectType::ARCHIVE;
    else if (OS::fileExists(filePath + "project.json")) Scratch::projectType = Scratch::ProjectType::EXTRACTED;
    // there is no Project selected => Main Menu
    else {
        threadFinished = true;
        return;
    };
    // open ScratchProject
    Result isFileOpen = readProjectJson();
    if (!isFileOpen.isSuccess() || !isFileOpen.value.has_value()) {
        error = isFileOpen.error;
        threadFinished = true;
        return;
    }
    nlohmann::json &projectJson = isFileOpen.value.value();
    // check if project-opt.json exist
    std::string projectCache = OS::getChacheFolderLocation() + std::to_string(std::hash<std::string>{}(filePath));
    if (Scratch::projectLocation == Scratch::ProjectLocation::EMBEDDED || !OS::fileExists(projectCache + "/project-opt.json") || !OS::fileExists(projectCache + "/version.txt"))
        goto skipOptimizedJSON;

    {
        std::size_t currentVersion = std::hash<nlohmann::json>{}(projectJson);
        std::ifstream file(projectCache + "/version.txt", std::ios::binary);

        // 3. If this triggers, we jump OUT of this scope.
        // C++ allows this (destructors for 'file' will be called automatically).
        if (!file) {
            goto skipOptimizedJSON;
        }

        try {
            size_t oldVersion;
            file >> oldVersion;
            if (oldVersion != currentVersion) goto skipOptimizedJSON;

            Result<std::string> fileResult = openFile(projectCache + "/project-opt.json");
            if (!fileResult.success) goto skipOptimizedJSON;

            try {
                projectJson = nlohmann::json::parse(fileResult.value.value());
            } catch (const std::exception &e) {
                Log::logError("Failed to parse extracted project.json: " + std::string(e.what()));
            }
        } catch (const std::exception &e) {
            Log::logError("Failed to load version.txt: " + std::string(e.what()));
        }
    }
skipOptimizedJSON:
    Runtime::loadScratchProject();
    threadFinished = true;

    loadingState = "Load data from Json";
    return;
}

Unzip::Result<nlohmann::json> Unzip::readProjectJson() {

    const std::string jsonPath = filePath + "project.json";

    if (Scratch::projectType == Scratch::ProjectType::EXTRACTED) {

        Result<std::string> fileResult = openFile(jsonPath);
        if (!fileResult.success)
            return Result<nlohmann::json>::failure(fileResult.error);

        try {
            nlohmann::json project_json = nlohmann::json::parse(fileResult.value.value());
            return Result<nlohmann::json>::success(project_json);
        } catch (const std::exception &e) {
            return Result<nlohmann::json>::failure(
                "Failed to parse extracted project.json: " + std::string(e.what()));
        }
    }

    else if (Scratch::projectType == Scratch::ProjectType::ARCHIVE) {

#ifdef USE_CMAKERC
        Unzip::Result<std::string> fileResult = openFile(jsonPath);
        if (!fileResult.success)
            return Result<nlohmann::json>::failure(fileResult.error);
        try {
            nlohmann::json project_json = nlohmann::json::parse(fileResult.value.value());
            return Result<nlohmann::json>::success(project_json);
        } catch (const std::exception &e) {
            return Result<nlohmann::json>::failure(
                "Failed to parse embedded SB3 project.json in RomFS: " + std::string(e.what()));
        }

#else
        return extractJson(filePath);
#endif
    }

    return Result<nlohmann::json>::failure("Unknown project type.");
}

Unzip::Result<std::string> Unzip::openFile(const std::string &path) {
#ifdef USE_CMAKERC
    try {
        const auto &file = fs.open(path);
        std::string content(file.begin(), file.begin() + file.size());
        return Result<std::string>::success(content);
    } catch (const std::exception &e) {
        return Result<std::string>::failure("failed to open file: " + std::string(e.what()));
    }
#else
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return Result<std::string>::failure("Cannot open file: " + path);

    std::ostringstream ss;
    ss << file.rdbuf();
    return Result<std::string>::success(ss.str());
#endif
}

Unzip::Result<nlohmann::json> Unzip::extractJson(const std::string &zipPath) {
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));

    if (!mz_zip_reader_init_file(&zipArchive, zipPath.c_str(), 0)) {
        return Result<nlohmann::json>::failure("Failed to initialize ZIP archive from file: " + zipPath);
    }

    int file_index = mz_zip_reader_locate_file(&zipArchive, "project.json", nullptr, 0);
    if (file_index < 0) {
        mz_zip_reader_end(&zipArchive);
        return Result<nlohmann::json>::failure("project.json not found in SB3 archive: " + zipPath);
    }

    size_t json_size = 0;
    void *json_data = mz_zip_reader_extract_to_heap(&zipArchive, file_index, &json_size, 0);
    if (!json_data) {
        mz_zip_reader_end(&zipArchive);
        return Result<nlohmann::json>::failure("Failed to extract project.json from SB3 archive: " + zipPath);
    }

    nlohmann::json project_json;
    try {
        project_json = nlohmann::json::parse(std::string(static_cast<char *>(json_data), json_size));
    } catch (const std::exception &e) {
        mz_free(json_data);
        mz_zip_reader_end(&zipArchive);
        return Result<nlohmann::json>::failure("Failed to parse project.json: " + std::string(e.what()));
    }

    mz_free(json_data);
    mz_zip_reader_end(&zipArchive);
    return Result<nlohmann::json>::success(project_json);
}

std::string Unzip::getSplashText() {
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

    // Replace {PlatformName} and {UserName} placeholders with actual values
    const std::string platformPlaceholder = "{PlatformName}";
    const std::string platform = OS::getPlatform();
    const std::string usernamePlaceholder = "{UserName}";
    const std::string username = Input::getUsername();

    size_t pos = 0;

    while ((pos = splash.find(platformPlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, platformPlaceholder.size(), platform);
        pos += platform.size(); // move past replacement
    }

    pos = 0;
    while ((pos = splash.find(usernamePlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, usernamePlaceholder.size(), username);
        pos += username.size(); // move past replacement
    }

    return splash;
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

bool Unzip::deleteProjectFolder(const std::string &directory) {
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

nlohmann::json Unzip::getProjectSetting(const std::string &settingName) {
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

void Unzip::loadConfigFile() {
    std::ifstream file(OS::getRomFSLocation() + "gfx/menu/config.json");
    if (!file.good()) {
        Log::logWarning("config file not found: 'romfs:/gfx/menu/config.json'");
        return;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::parse_error &e) {
        Log::logError("Failed to parse config.json file: " + std::string(e.what()));
        file.close();
        return;
    }
    file.close();
    if (!json.contains("embeddedPath")) configFile.embeddedPath = OS::getRomFSLocation() + json["embeddedPath"].get<std::string>();
    if (!json.contains("blockUpdatesPerFrame")) configFile.blockUpdatesPerFrame = json["blockUpdatesPerFrame"].get<int>();
    if (!json.contains("cacheFolder")) configFile.cacheFolder = OS::getFilesystemRootPrefix() + json["cacheFolder"].get<std::string>();
    if (configFile.cacheFolder == "") configFile.cacheFolder = OS::getChacheFolderLocation();
    std::string username = loadUsernameFromSettings();
    if (username == "") {
        if (!json.contains("username")) {
            configFile.username = OS::getRomFSLocation() + json["username"].get<std::string>();
            std::string newUsername = "";
            for (char c : configFile.username) {
                if (c == '$') {
                    newUsername += std::to_string(rand() % 10);
                } else {
                    newUsername += c;
                }
            }
            configFile.username = newUsername;
        } else {
            configFile.username = "User" + std::to_string(rand() % 1000);
        }
    } else {
        configFile.username = username;
    }
}