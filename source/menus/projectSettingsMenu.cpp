#include "projectSettingsMenu.hpp"
#include "menuManager.hpp"
#include "unpackMenu.hpp"
#include <clay.h>
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
            menuManager->queueChangeMenu(MenuID::ControlsMenu, const_cast<void *>(static_cast<const void *>(projectName.c_str())));
        }
    }

    if (unpackedExists) {
        renderButton("deleteUnpacked");
        if (isButtonJustPressed("deleteUnpacked")) {
            UnpackParams *params = new UnpackParams;
            params->projectName = projectName;
            params->deletingProject = true;
            menuManager->queueChangeMenu(MenuID::UnpackMenu, params);
        }
    } else {
        renderButton("unpackProject");
        if (isButtonJustPressed("unpackProject")) {
            UnpackParams *params = new UnpackParams;
            params->projectName = projectName;
            params->deletingProject = false;
            menuManager->queueChangeMenu(MenuID::UnpackMenu, params);
        }
    }
}
