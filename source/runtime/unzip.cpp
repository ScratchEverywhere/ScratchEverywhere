#include "unzip.hpp"
#include "input.hpp"
#include "os.hpp"
#include "parser.hpp"
#include "runtime.hpp"
#include "translation.hpp"
#include <cstring>
#include <ctime>
#include <errno.h>
#include <filesystem.hpp>
#include <fstream>
#include <image.hpp>
#include <istream>
#include <log.hpp>
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
#include <thread.hpp>
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
std::unique_ptr<ZipArchive> Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;
std::unique_ptr<ProjectLoader> Unzip::loader;
std::optional<ProjectFormat> Unzip::unpackedFormatHint;
bool Unzip::UnpackedInSD = false;

ProjectFormat Unzip::detectFormat(const std::string &path) {
    if (Unzip::unpackedFormatHint.has_value()) {
        ProjectFormat format = *Unzip::unpackedFormatHint;
        Unzip::unpackedFormatHint.reset();
        return format;
    }

    const auto hasExtension = [&](const std::string &ext) {
        return path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
    };

    if (hasExtension(".sb2")) return ProjectFormat::SB2;
    if (hasExtension(".sb")) return ProjectFormat::SB1;
    return ProjectFormat::SB3;
}

std::string Unzip::resolveZipProjectPath(const std::string &baseNoExt, ProjectFormat &format) {
    if (FileSystem::fileExists(baseNoExt + ".sb3")) {
        format = ProjectFormat::SB3;
        return baseNoExt + ".sb3";
    }
    if (FileSystem::fileExists(baseNoExt + ".sb2")) {
        format = ProjectFormat::SB2;
        return baseNoExt + ".sb2";
    }
    if (FileSystem::fileExists(baseNoExt + ".sb")) {
        format = ProjectFormat::SB1;
        return baseNoExt + ".sb";
    }
    format = ProjectFormat::SB3;
    return "";
}

int Unzip::openFile(std::istream *&file, ProjectFormat format) {
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
    std::string cmrcUnzipped = OS::normalizeCMRCPath(unzippedPath);
    if (fs.exists(cmrcUnzipped)) {
        const auto &romfsFile = fs.open(cmrcUnzipped);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
    }
#else
    file = new std::ifstream(unzippedPath, std::ios::binary | std::ios::ate);
#endif
    Scratch::projectType = ProjectType::UNZIPPED;
    if (file != nullptr) {
        if (*file) return 1;
        else {
            delete file;
            file = nullptr;
        }
    }
    // .sb3 Project in romfs:/
    Log::logWarning("No unzipped project, trying embedded.");
    Scratch::projectType = ProjectType::EMBEDDED;
#ifdef USE_CMAKERC
    std::string cmrcEmbedded = OS::normalizeCMRCPath(embeddedFilename);
    if (fs.exists(cmrcEmbedded)) {
        const auto &romfsFile = fs.open(cmrcEmbedded);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
        file->clear();
        file->seekg(0, std::ios::beg);
    }
#else
    file = new std::ifstream(embeddedFilename, std::ios::binary | std::ios::ate);
#endif
    if (file != nullptr) {
        if (*file) {
            Unzip::filePath = embeddedFilename;
            return 1;
        } else {
            delete file;
            file = nullptr;
        }
    }
    // Main menu
    Log::logWarning("No sb3 project, trying Main Menu.");
    Scratch::projectType = ProjectType::UNEMBEDDED;
    if (filePath == "") {
        Log::log("Activating main menu...");
        return -1;
    }
    // SD card Project
    Log::logWarning("Main Menu already done, loading SD card project.");

    if (format == ProjectFormat::SB1) {
        Log::log(".sb project in SD card");
        file = new std::ifstream(filePath, std::ios::binary | std::ios::ate);
        if (file == nullptr || !(*file)) {
            Log::logCritical("Couldnt find Scratch project file: " + filePath + " jinkies.", true);
            return 0;
        }
        return 1;
    }

    if (filePath.size() >= 4 && (filePath.compare(filePath.size() - 4, 4, ".sb3") == 0 || filePath.compare(filePath.size() - 4, 4, ".sb2") == 0)) {
        Log::log("Normal packed project in SD card");
        file = new std::ifstream(filePath, std::ios::binary | std::ios::ate);
        if (file == nullptr || !(*file)) {
            Log::logCritical("Couldnt find Scratch project file: " + filePath + " jinkies.", true);
            return 0;
        }

        return 1;
    }
    Scratch::projectType = ProjectType::UNZIPPED;
    Log::log("Unpacked project in SD card");
    // check if Unpacked Project
    file = new std::ifstream(filePath + "/project.json", std::ios::binary | std::ios::ate);
    if (file == nullptr || !(*file)) {
        Log::logCritical("Couldnt open unpacked Scratch project: " + filePath, true);
        return 0;
    }
    filePath = filePath + "/";
    UnpackedInSD = true;

    return 1;
}

void projectLoaderThread(void *data) {
    Unzip::openScratchProject(NULL);
}

void loadInitialImages() {
    Unzip::loadingState = TranslationManager::getTranslation("ui.loading.images");
    for (auto &currentSprite : Scratch::sprites) {

        Scratch::loadCurrentCostumeImage(currentSprite);
    }
}

bool Unzip::load() {
    Unzip::threadFinished = false;
    Unzip::projectOpened = 0;

#if defined(ENABLE_LOADSCREEN) && defined(ENABLE_MENU)

    SE_Thread projectThread;
    if (projectThread.create(projectLoaderThread, nullptr, 0x262144, 0, -1, "ProjectLoader")) {
        Loading loading;
        loading.init();

        while (!Unzip::threadFinished) {
            loading.render();
        }

        projectThread.join();
        loading.cleanup();

        if (Unzip::projectOpened != 1) {
            return false;
        }
    } else {
        Unzip::openScratchProject(nullptr);
        if (Unzip::projectOpened != 1) {
            return false;
        }
    }

#else
    // Non-threaded loading fallback
    Unzip::openScratchProject(nullptr);
    if (Unzip::projectOpened != 1) {
        return false;
    }
#endif
    loadInitialImages();
    return true;
}

void Unzip::openScratchProject(void *arg) {
    loadingState = TranslationManager::getTranslation("ui.loading.opening");
    Unzip::UnpackedInSD = false;
    std::istream *file = nullptr;

    ProjectFormat format = Unzip::detectFormat(Unzip::filePath);

    int isFileOpen = openFile(file, format);
    if (isFileOpen == 0) {
        Log::logCritical("Failed to open Scratch project.", true);
        Unzip::projectOpened = -1;
        Unzip::threadFinished = true;
        return;
    } else if (isFileOpen == -1) {
        Log::log("Main Menu activated.");
        Unzip::projectOpened = -3;
        Unzip::threadFinished = true;
        return;
    }
    loadingState = TranslationManager::getTranslation("ui.loading.unzipping");
    Unzip::loader = createProjectLoader(format);
    bool loaded = Unzip::loader->load(file);
    delete file;
    if (!loaded) {
        Log::logCritical("Failed to load Scratch project.", false);
        Unzip::projectOpened = -2;
        Unzip::threadFinished = true;
        return;
    }

    Unzip::projectOpened = 1;
    Unzip::threadFinished = true;
    return;
}

std::vector<std::string> Unzip::getProjectFiles(const std::string &directory) {
    struct stat dirStat;

    if (stat(directory.c_str(), &dirStat) != 0) {
        Log::logWarning("Directory does not exist! " + directory);
        auto potentialError = FileSystem::createDirectory(directory);
        if (!potentialError.has_value()) Log::logWarning("Failed to create directory, " + directory + ", " + potentialError.error());
        return {};
    }

    if (!(dirStat.st_mode & S_IFDIR)) {
        Log::logWarning("Path is not a directory! " + directory);
        return {};
    }

    auto projectFiles = FileSystem::listDirectory(directory);
    if (!projectFiles.has_value()) {
        Log::logCritical("Error while reading project files: " + projectFiles.error(), true);
        return {};
    }

    projectFiles.value().erase(std::remove_if(projectFiles.value().begin(), projectFiles.value().end(), [](const std::string &file) {
                                   const auto hasExtension = [&](const std::string &ext) {
                                       return file.size() >= ext.size() && file.compare(file.size() - ext.size(), ext.size(), ext) == 0;
                                   };
                                   return !hasExtension(".sb3") && !hasExtension(".sb2") && !hasExtension(".sb");
                               }),
                               projectFiles.value().end());

    std::sort(projectFiles.value().begin(), projectFiles.value().end(), [](const std::string &a, const std::string &b) {
        return std::lexicographical_compare(
            a.begin(), a.end(),
            b.begin(), b.end(),
            [](char x, char y) { return std::tolower(x) < std::tolower(y); });
    });

    return projectFiles.value();
}

void *Unzip::getFileInSB3(const std::string &fileName, size_t *outSize) {
    if (!Unzip::loader) return nullptr;
    return Unzip::loader->getAsset(fileName, outSize);
}

bool Unzip::extractProject(const std::string &zipPath, const std::string &destFolder) {
    auto zip = createZipArchive();
    if (!zip->openFile(zipPath)) {
        Log::logCritical("Failed to open zip: " + zipPath, true);
        return false;
    }

    auto potentialError = FileSystem::createDirectory(destFolder + "/");
    if (!potentialError.has_value()) {
        Log::logError(potentialError.error());
        return false;
    }

    bool hadError = false;
    bool walked = zip->extractAll([&](const std::string &filename) -> std::string {
        if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
            return "";

        std::string outPath = destFolder + "/" + filename;

        auto potentialError = FileSystem::createDirectory(FileSystem::parentPath(outPath));
        if (!potentialError.has_value()) {
            Log::logError(potentialError.error());
            hadError = true;
            return "";
        }

        return outPath;
    });

    if (!walked || hadError) {
        Log::logCritical("Failed to extract zip: " + zipPath, false);
        return false;
    }

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

    auto potentialError = FileSystem::removeDirectory(directory);
    if (!potentialError.has_value()) {
        Log::logCritical(std::string("Failed to delete folder: ") + potentialError.error(), false);
        return false;
    }

    return true;
}

nlohmann::json Unzip::getSetting(const std::string &settingName) {
    std::string folderPath = filePath + ".json";
    std::string content;

    if (Scratch::projectType != ProjectType::UNEMBEDDED) {
#ifdef USE_CMAKERC
        const auto &fs = cmrc::romfs::get_filesystem();
        std::string cmrcPath = OS::normalizeCMRCPath(folderPath);

        if (!fs.exists(cmrcPath)) {
            Log::logWarning("Project settings file not found: romfs:/" + folderPath);
            return nlohmann::json();
        }

        const auto &file = fs.open(cmrcPath);
        content.assign(file.begin(), file.end());
#else
        std::ifstream file(OS::getRomFSLocation() + "project.sb3.json");
        if (!file.is_open()) {
            Log::logWarning("Project settings file not found in RomFS.");
            return nlohmann::json();
        }
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
#endif
    } else {
        std::ifstream file(folderPath);
        if (!file.is_open()) {
            Log::logWarning("Project settings file not found: " + folderPath);
            return nlohmann::json();
        }
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }

    nlohmann::json json = nlohmann::json::parse(content, nullptr, false);

    if (json.is_discarded()) {
        Log::logCritical("Failed to parse JSON file: Syntax error.", false);
        return nlohmann::json();
    }

    if (json.contains("settings") && json["settings"].contains(settingName)) {
        return json["settings"][settingName];
    }

    return nlohmann::json();
}
