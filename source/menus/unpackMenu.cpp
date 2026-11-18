#include "unpackMenu.hpp"
#include "log.hpp"
#include "menuManager.hpp"
#include "os.hpp"
#include "thread.hpp"
#include "translation.hpp"
#include "unzip.hpp"
#include <filesystem.hpp>

UnpackMenu::UnpackMenu(const std::string &projectPath) {
    MenuManager::canChangeTabs = false;
    this->projectPath = projectPath;
}

UnpackMenu::~UnpackMenu() {
    MenuManager::canChangeTabs = true;
}

void UnpackMenu::callback(UI::Builder &b) {
    const std::string projectName = projectPath.substr(0, projectPath.size() - 4);
    const std::string sb3Path = OS::getScratchFolderLocation() + projectPath;
    const std::string unpackedPath = OS::getScratchFolderLocation() + "cache/" + projectName;

    bool shouldUnpack = false;
    if (FileSystem::fileExists(unpackedPath)) {
        if (FileSystem::fileModifiedTime(sb3Path) > FileSystem::fileModifiedTime(unpackedPath)) {
            shouldUnpack = true;
        }
    } else shouldUnpack = true;

    if (shouldUnpack) {
        if (Unzip::extractProject(sb3Path, unpackedPath)) {
            goto load;
        } else {
            Log::logError("Failed to unpack project!");
            MenuManager::changeMenu(MenuId::ProjectMenu);
            MenuManager::toUnload = MenuId::UnpackMenu;
        }
    } else {
        Log::log("Unpacked project already exists, no need to unpack");
    load:
        Unzip::filePath = unpackedPath;
        MenuManager::projectPendingLoad = true;
        MenuManager::toUnload = MenuId::UnpackMenu;
    }
}

void UnpackMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({
                      .text = TranslationManager::getTranslation("ui.unpack.wait"),
                      .color = {255, 255, 255, 255},
                  });
                  b.text({
                      .text = TranslationManager::getTranslation("ui.unpack.warning"),
                      .color = {255, 255, 255, 255},
                  });
              });
}