#include "blockUtils.hpp"
#include "unzip.hpp"
#include <filesystem.hpp>
#include <log.hpp>

SCRATCH_BLOCK(sceneManager, receivedData) {
    *outValue = Scratch::dataNextProject;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sceneManager, openSB3) {
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, Unzip::filePath)) return BlockResult::REPEAT;

    Log::log("[SceneManager] Open next Project with Block");
    Scratch::nextProject = true;
    if (Unzip::filePath.rfind("sd:", 0) == 0) {
        const std::string drivePrefix = OS::getFilesystemRootPrefix();
        Unzip::filePath.replace(0, 3, drivePrefix);
    } else if (Unzip::filePath.rfind("romfs:", 0) == 0) {
        const std::string drivePrefix = OS::getRomFSLocation();
        Unzip::filePath.replace(0, 6, drivePrefix);
    } else {
        Unzip::filePath = OS::getScratchFolderLocation() + Unzip::filePath;
    }

    if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
        Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
    }
    if (!FileSystem::fileExists(Unzip::filePath + "/project.json"))
        Unzip::filePath = Unzip::filePath + ".sb3";

    Scratch::dataNextProject = Value();
    Scratch::shouldStop = true;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(sceneManager, openSB3withData) {
    Value arg1;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, Unzip::filePath) ||
        !Scratch::getInputValue(block, "arg1", thread, sprite, arg1)) return BlockResult::REPEAT;

    Log::log("[SceneManager] Open next Project with Block and data");
    Scratch::nextProject = true;
    if (Unzip::filePath.rfind("sd:", 0) == 0) {
        const std::string drivePrefix = OS::getFilesystemRootPrefix();
        Unzip::filePath.replace(0, 3, drivePrefix);
    } else if (Unzip::filePath.rfind("romfs:", 0) == 0) {
        const std::string drivePrefix = OS::getRomFSLocation();
        Unzip::filePath.replace(0, 6, drivePrefix);
    } else {
        Unzip::filePath = OS::getScratchFolderLocation() + Unzip::filePath;
    }
    if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
        Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
    }
    if (!FileSystem::fileExists(Unzip::filePath + "/project.json"))
        Unzip::filePath = Unzip::filePath + ".sb3";

    Scratch::dataNextProject = arg1;
    Scratch::shouldStop = true;
    return BlockResult::RETURN;
}
