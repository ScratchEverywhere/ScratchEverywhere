#include "unpackMenu.hpp"
#include "log.hpp"
#include "menuManager.hpp"
#include "os.hpp"
#include "renderer.hpp"
#include "translation.hpp"
#include "types.hpp"
#include "unzip.hpp"
#include <filesystem.hpp>

static void unpackWorker(void *arg) {
    UnpackTask *task = static_cast<UnpackTask *>(arg);

    bool shouldUnpack = false;
    if (FileSystem::fileExists(task->unpackedPath)) {
        if (FileSystem::fileModifiedTime(task->sb3Path) > FileSystem::fileModifiedTime(task->unpackedPath)) {
            shouldUnpack = true;
        }
    } else {
        shouldUnpack = true;
    }

    bool success = true;
    if (shouldUnpack) {
        success = Unzip::extractProject(task->sb3Path, task->unpackedPath);
    }

    task->mtx.lock();
    task->alreadyUnpacked = !shouldUnpack;
    task->success = success;
    task->done = true;
    task->mtx.unlock();
}

UnpackMenu::UnpackMenu(const std::string &projectPath) {
    MenuManager::canChangeTabs = false;
    this->projectPath = projectPath;

    const std::string projectName = projectPath.substr(0, projectPath.size() - 4);
    unpackedPath = OS::getScratchFolderLocation() + "cache/" + projectName;

    task.sb3Path = OS::getScratchFolderLocation() + projectPath;
    task.unpackedPath = unpackedPath;
    task.mtx.init();

    if (thread.create(unpackWorker, &task, 32 * 1024, 1, -1, "UnpackThread")) {
        threadStarted = true;
    } else {
        unpackWorker(&task);
    }

    thing.velX = 3.4;
    thing.velY = 1.2;
    thing.x = 100;
    thing.y = 50;
}

UnpackMenu::~UnpackMenu() {
    if (threadStarted) {
        thread.join();
    }
    MenuManager::canChangeTabs = true;
}

void UnpackMenu::callback(UI::Builder &b) {

    thing.x += thing.velX;
    thing.y += thing.velY;
    thing.velY += 0.15;
    if (thing.x < 0) thing.velX += 3.0;
    if (thing.x + thing.width > Render::getWidth()) thing.velX -= 3.0;
    if (thing.y < 0) thing.velY += 14.0;
    if (thing.y + thing.height > Render::getHeight()) thing.velY -= 5.0;
    b.dirty = true;

    task.mtx.lock();
    const bool done = task.done;
    const bool success = task.success;
    const bool alreadyUnpacked = task.alreadyUnpacked;
    task.mtx.unlock();

    if (!done) return;

    if (success) {
        if (alreadyUnpacked) {
            Log::log("Unpacked project already exists, no need to unpack");
        }
        Unzip::filePath = unpackedPath;
        MenuManager::projectPendingLoad = true;
        MenuManager::toUnload = MenuId::UnpackMenu;
    } else {
        Log::logError("Failed to unpack project!");
        MenuManager::changeMenu(MenuId::ProjectMenu);
        MenuManager::toUnload = MenuId::UnpackMenu;
    }
}

void UnpackMenu::build(UI::Builder &b) {
    b.element({
        .flags = UI::Flag::Floating,
        .w = UI::Size::fixed(thing.width),
        .h = UI::Size::fixed(thing.height),
        .x = static_cast<int16_t>(thing.x),
        .y = static_cast<int16_t>(thing.y),
        .color = {255, 255, 255, 255},
    });

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