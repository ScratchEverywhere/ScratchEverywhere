#include <log.hpp>
#include <os.hpp>
#include <switch.h>

static char nickname[0x21];

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    AccountUid userID = {0};
    AccountProfile profile;
    AccountProfileBase profilebase;
    memset(&profilebase, 0, sizeof(profilebase));

    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        Log::logError("Failed to init romfs."); // TODO: Include error code
        goto postAccount;
    }

    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        Log::logError("accountInitialize failed.");
        goto postAccount;
    }

    rc = accountGetPreselectedUser(&userID);
    if (R_FAILED(rc)) {
        PselUserSelectionSettings settings;
        memset(&settings, 0, sizeof(settings));
        rc = pselShowUserSelector(&userID, &settings);
        if (R_FAILED(rc)) {
            Log::logError("pselShowUserSelector failed.");
            goto postAccount;
        }
    }

    rc = accountGetProfile(&profile, userID);
    if (R_FAILED(rc)) {
        Log::logError("accountGetProfile failed.");
        goto postAccount;
    }

    rc = accountProfileGet(&profile, NULL, &profilebase);
    if (R_FAILED(rc)) {
        Log::logError("accountProfileGet failed.");
        goto postAccount;
    }

    memset(nickname, 0, sizeof(nickname));
    strncpy(nickname, profilebase.nickname, sizeof(nickname) - 1);

    socketInitializeDefault();

    accountProfileClose(&profile);
    accountExit();
postAccount:
    return true;
}

void OS::deinit() {
    romfsExit();
}

std::string OS::getPlatform() {
    return "Switch";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return "/switch/scratch-nx/";
}

std::string OS::getRomFSLocation() {
    return "romfs:/";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    if (std::string(nickname) != "") {
        return std::string(nickname);
    }
    return "Player";
}