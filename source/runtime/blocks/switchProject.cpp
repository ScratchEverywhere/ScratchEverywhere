#include "blockUtils.hpp"
#include "unzip.hpp"

SCRATCH_BLOCK(switchProject, receivedData) {
    *outValue = Scratch::dataNextProject;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(switchProject, openSB3) {
    Value arg0;
    if (!Scratch::getInput(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::log("Open next Project with Block");
    Scratch::nextProject = true;
    Unzip::filePath = arg0.asString();
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
    if (!OS::fileExists(Unzip::filePath + "/project.json"))
        Unzip::filePath = Unzip::filePath + ".sb3";

    Scratch::dataNextProject = Value();
    Scratch::shouldStop = true;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(switchProject, openSB3withData) {
    Value arg0, arg1;
    if (!Scratch::getInput(block, "arg0", thread, sprite, arg0) ||
        !Scratch::getInput(block, "arg0", thread, sprite, arg1)) return BlockResult::REPEAT;

    Log::log("Open next Project with Block and data");
    Scratch::nextProject = true;
    Unzip::filePath = arg0.asString();
    // if filepath contains sd:/ at the beginning and only at the beginning, replace it with sdmc:/
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
    if (!OS::fileExists(Unzip::filePath + "/project.json"))
        Unzip::filePath = Unzip::filePath + ".sb3";

    Scratch::dataNextProject = arg1;
    Scratch::shouldStop = true;
    return BlockResult::RETURN;
}