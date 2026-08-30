#include <os.hpp>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#if defined(RENDERER_SDL2)

#elif defined(RENDERER_SDL3)
// todo
#endif

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    UIUserInterfaceIdiom device = [[UIDevice currentDevice] userInterfaceIdiom];
    
    switch (device) {
        case UIUserInterfaceIdiomPhone:
            return "iPhone";
        case UIUserInterfaceIdiomPad:
            return "iPad";
        case UIUserInterfaceIdiomTV:
            return "Apple TV";
        default:
            return "iOS";
    }
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

    NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    const char *path = [[dirs objectAtIndex:0] fileSystemRepresentation];
    return std::string(path) + "/";
}

std::string OS::getRomFSLocation() {
    NSBundle *bundle = [NSBundle mainBundle];
    const char *romfsDir = [[bundle resourcePath] fileSystemRepresentation];
    return std::string(romfsDir) + "/";
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
    return "Player";
}
