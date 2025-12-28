#include "unzip.hpp"
#include "input.hpp"
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fstream>
#include <image.hpp>
#include <istream>
#include <menus/loading.hpp>
#include <random>
#include <settings.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef ENABLE_LOADSCREEN
#ifdef __3DS__
#include <3ds.h>
#elif defined(RENDERER_SDL1)
#include "SDL/SDL.h"
#elif defined(RENDERER_SDL2) || defined(OPENGL_WINDOWING_SDL2)
#include "SDL2/SDL.h"
#elif defined(RENDERER_SDL3) || defined(OPENGL_WINDOWING_SDL3)
#include "SDL3/SDL.h"
#else
#include <thread>
#endif
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
#include <sstream>

CMRC_DECLARE(romfs);
#endif

volatile int Unzip::projectOpened = 0;
std::string Unzip::loadingState = "";
volatile bool Unzip::threadFinished = false;
std::string Unzip::filePath = "";
mz_zip_archive Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;
bool Unzip::UnpackedInSD = false;

int Unzip::openFile(std::istream *&file) {
    Log::log("Unzipping Scratch project...");

    // load Scratch project into memory
    Log::log("Loading SB3 into memory...");
    std::string embeddedFilename = "project.sb3";
    std::string unzippedPath = "project/project.json";

    embeddedFilename = OS::getRomFSLocation() + embeddedFilename;
    unzippedPath = OS::getRomFSLocation() + unzippedPath;

#ifdef USE_CMAKERC
    const auto &fs = cmrc::romfs::get_filesystem();
#endif

    // Unzipped Project in romfs:/
#ifdef USE_CMAKERC
    if (fs.exists(unzippedPath)) {
        const auto &romfsFile = fs.open(unzippedPath);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
    }
#else
    file = new std::ifstream(unzippedPath, std::ios::binary | std::ios::ate);
#endif
    Scratch::projectType = UNZIPPED;
    if (file != nullptr && *file) return 1;
    // .sb3 Project in romfs:/
    Log::logWarning("No unzipped project, trying embedded.");
    Scratch::projectType = EMBEDDED;
#ifdef USE_CMAKERC
    if (fs.exists(embeddedFilename)) {
        const auto &romfsFile = fs.open(embeddedFilename);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
        file->seekg(0, std::ios::end);
    }
#else
    file = new std::ifstream(embeddedFilename, std::ios::binary | std::ios::ate);
#endif
    if (file != nullptr && *file) return 1;
    // Main menu
    Log::logWarning("No sb3 project, trying Main Menu.");
    Scratch::projectType = UNEMBEDDED;
    if (filePath == "") {
        Log::log("Activating main menu...");
        return -1;
    }
    // SD card Project
    Log::logWarning("Main Menu already done, loading SD card project.");
    // check if normal Project
    if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4, filePath.size()) == ".sb3") {
        Log::log("Normal .sb3 project in SD card ");
        file = new std::ifstream(filePath, std::ios::binary | std::ios::ate);
        if (file == nullptr || !(*file)) {
            Log::logError("Couldnt find Scratch project file: " + filePath + " jinkies.");
            return 0;
        }

        return 1;
    }
    Scratch::projectType = UNZIPPED;
    Log::log("Unpacked .sb3 project in SD card");
    // check if Unpacked Project
    file = new std::ifstream(filePath + "/project.json", std::ios::binary | std::ios::ate);
    if (file == nullptr || !(*file)) {
        Log::logError("Couldnt open unpacked Scratch project: " + filePath);
        return 0;
    }
    filePath = filePath + "/";
    UnpackedInSD = true;

    return 1;
}

int projectLoaderThread(void *data) {
    Unzip::openScratchProject(NULL);
    return 0;
}

void loadInitialImages() {
    Unzip::loadingState = "Loading images";
    int sprIndex = 1;
    if (Scratch::projectType == UNZIPPED) {
        for (auto &currentSprite : Scratch::sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(Scratch::sprites.size());
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName, currentSprite);
            sprIndex++;
        }
    } else {
        for (auto &currentSprite : Scratch::sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(Scratch::sprites.size());
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName, currentSprite);
            sprIndex++;
        }
    }
}

bool Unzip::load() {

    Unzip::threadFinished = false;
    Unzip::projectOpened = 0;

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
        Unzip::threadFinished = true;
        Unzip::projectOpened = -3;
    }

    Loading loading;
    loading.init();

    while (!Unzip::threadFinished) {
        loading.render();
    }
    threadJoin(projectThread, U64_MAX);
    threadFree(projectThread);
    if (Unzip::projectOpened != 1) {
        loading.cleanup();
        return false;
    }
    loading.cleanup();
    osSetSpeedupEnable(false);

#elif defined(RENDERER_SDL1) | defined(RENDERER_SDL2) || defined(OPENGL_WINDOWING_SDL2) || defined(RENDERER_SDL3) || defined(OPENGL_WINDOWING_SDL3) // create SDL thread for loading screen
#ifdef RENDERER_SDL1
    SDL_Thread *thread = SDL_CreateThread(projectLoaderThread, nullptr);
#elif defined(RENDERER_SDL2) || defined(OPENGL_WINDOWING_SDL2)
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

    if (Unzip::projectOpened != 1)
        return false;
#else // create thread for loading screen
    std::thread thread(projectLoaderThread, nullptr);
    if (thread.joinable()) {
        Loading loading;
        loading.init();

        while (!Unzip::threadFinished) {
            loading.render();
        }
        thread.join();
        loading.cleanup();
    } else Unzip::openScratchProject(NULL);

    if (Unzip::projectOpened != 1)
        return false;
#endif
#else

    // non-threaded loading
    Unzip::openScratchProject(NULL);
    if (Unzip::projectOpened != 1)
        return false;
#endif

    loadInitialImages();
    return true;
}

void Unzip::openScratchProject(void *arg) {
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
    Parser::loadSprites(project_json);
    Unzip::projectOpened = 1;
    Unzip::threadFinished = true;
    delete file;
    return;
}

std::vector<std::string> Unzip::getProjectFiles(const std::string &directory) {
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
    std::string username = OS::getUsername();
    nlohmann::json json = SettingsManager::getConfigSettings();
    if (json.contains("EnableUsername") && json["EnableUsername"].is_boolean() && json["EnableUsername"].get<bool>()) {
        if (json.contains("Username") && json["Username"].is_string()) {
            std::string customUsername = json["Username"].get<std::string>();
            if (!customUsername.empty()) {
                username = customUsername;
            }
        }
    }

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

nlohmann::json Unzip::unzipProject(std::istream *file) {
    nlohmann::json project_json;

    if (Scratch::projectType != UNZIPPED) {
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

bool Unzip::extractProject(const std::string &zipPath, const std::string &destFolder) {
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

nlohmann::json Unzip::getSetting(const std::string &settingName) {
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
