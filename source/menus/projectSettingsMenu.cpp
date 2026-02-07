#include "projectSettingsMenu.hpp"
#include "menuManager.hpp"
#include "unpackMenu.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <settings.hpp>

ProjectSettingsMenu::ProjectSettingsMenu(void *userdata) {
    projectName = static_cast<char *>(userdata);
    settings = SettingsManager::getProjectSettings(projectName);

    if (SettingsManager::isProjectUnpacked(projectName)) unpackedExists = true;
    else unpackedExists = false;

    if (!settings.contains("bottomScreen")) settings["bottomScreen"] = false;

    init("Project Settings");
}

ProjectSettingsMenu::~ProjectSettingsMenu() {
    SettingsManager::saveProjectSettings(settings, projectName);
}

void ProjectSettingsMenu::renderSettings() {

    if (Input::isButtonJustPressed("B") && menuManager->canChangeMenus) {
        menuManager->back();
        return;
    }

#if defined(__3DS__) || defined(__NDS__)
    renderToggle("bottomScreen");
#endif

    if (Input::isControllerConnected()) {
        renderButton("changeControls");
        if (isButtonJustPressed("changeControls")) {
            menuManager->changeMenu(MenuID::ControlsMenu, const_cast<void *>(static_cast<const void *>(projectName.c_str())));
            return;
        }
    }

    if (unpackedExists) {
        renderButton("deleteUnpacked");
        if (isButtonJustPressed("deleteUnpacked")) {
            UnpackParams params;
            params.projectName = projectName;
            params.deletingProject = true;
            menuManager->changeMenu(MenuID::UnpackMenu, &params);
            return;
        }
    } else {
        renderButton("unpackProject");
        if (isButtonJustPressed("unpackProject")) {
            UnpackParams params;
            params.projectName = projectName;
            params.deletingProject = false;
            menuManager->changeMenu(MenuID::UnpackMenu, &params);
            return;
        }
    }
}
