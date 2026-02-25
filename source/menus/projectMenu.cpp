#include "projectMenu.hpp"
#include "projectSettings.hpp"
#include "settings.hpp"
#include "unpackMenu.hpp"
#include <audio.hpp>

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {
#if defined(__NDS__)
    if (!SoundPlayer::isSoundLoaded("gfx/menu/mm_ds.wav")) {
        SoundPlayer::startSoundLoaderThread(nullptr, nullptr, "gfx/menu/mm_ds.wav", false, false);
    }
#else
    if (!SoundPlayer::isSoundLoaded("gfx/menu/mm_splash.ogg")) {
        SoundPlayer::startSoundLoaderThread(nullptr, nullptr, "gfx/menu/mm_splash.ogg", true, false);
        SoundPlayer::stopSound("gfx/menu/mm_splash.ogg");
    }
#endif

    projectControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->needsToBeSelected = false;
    backButton->scale = 1.0;

    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
    UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

    // initialize text and set positions
    int yPosition = 30;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file.substr(0, file.length() - 4), "gfx/menu/projectBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(0, 0, 0, 255));
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);

        ButtonObject *settingsButton = new ButtonObject("", "gfx/menu/projectSettings.svg", 140, project->y, "gfx/menu/Ubuntu-Bold");
        projects.push_back(settingsButton);
        projectControl->buttonObjects.push_back(settingsButton);

        project->buttonRight = settingsButton;
        settingsButton->buttonLeft = project;

        yPosition += 50;
    }
    for (std::string &file : UnzippedFiles) {
        ButtonObject *project = new ButtonObject(file, "gfx/menu/projectBoxFast.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(126, 101, 1, 255));
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);

        ButtonObject *settingsButton = new ButtonObject("", "gfx/menu/projectSettings.svg", 140, project->y, "gfx/menu/Ubuntu-Bold");
        projects.push_back(settingsButton);
        projectControl->buttonObjects.push_back(settingsButton);

        project->buttonRight = settingsButton;
        settingsButton->buttonLeft = project;

        yPosition += 50;
    }

    for (size_t i = 0; i < projects.size(); i++) {
        // Check if there's a project above
        if (i > 1) {
            projects[i]->buttonUp = projects[i - 2];
        }

        // Check if there's a project below
        if (i < projects.size() - 1 && i < projects.size() - 2) {
            projects[i]->buttonDown = projects[i + 2];
        }
    }

    // check if user has any projects
    if (projectFiles.size() == 0 && UnzippedFiles.size() == 0) {
        hasProjects = false;
        noProjectsButton = new ButtonObject("", "gfx/menu/noProjects.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
        projectControl->selectedObject = noProjectsButton;
        projectControl->selectedObject->isSelected = true;
        noProjectsText = createTextObject("No Scratch projects found!", 0, 0);
        noProjectsText->setCenterAligned(true);
        noProjectInfo = createTextObject("a", 0, 0);
        noProjectInfo->setCenterAligned(true);

#ifdef __WIIU__
        noProjectInfo->setText("Put Scratch projects in sd:/wiiu/scratch-wiiu/ !");
#elif defined(__3DS__)
        noProjectInfo->setText("Put Scratch projects in sd:/3ds/scratch-everywhere/ !");
#elif defined(WII)
        noProjectInfo->setText("Put Scratch projects in sd:/apps/scratch-wii/ !");
#elif defined(VITA)
        noProjectInfo->setText("Put Scratch projects in ux0:data/scratch-vita/ !");
#elif defined(GAMECUBE)
        noProjectInfo->setText("Put Scratch projects in sd:/scratch-gamecube/ !");
#elif defined(__SWITCH__)
        noProjectInfo->setText("Put Scratch projects in sd:/switch/scratch-nx !");
#elif defined(__NDS__)
        noProjectInfo->setText("Put Scratch projects in sd:/scratch-ds !");
#elif defined(__PS4__)
        noProjectInfo->setText("Put Scratch projects in /data/scratch-ps4 !");
#elif defined(WEBOS)
        noProjectInfo->setText("Upload Scratch projects to apps/usr/palm/applications/io.github.scratcheverywhere/projects/ using webosbrew Dev Manager!");
#else
        noProjectInfo->setText("Put Scratch projects in the scratch-everywhere folder!");
#endif

        if (noProjectInfo->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectInfo->getSize()[0] * 1.15);
            noProjectInfo->setScale(scale);
        }
        if (noProjectsText->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectsText->getSize()[0] * 1.15);
            noProjectsText->setScale(scale);
        }

    } else {
        projectControl->enableScrolling = true;
        projectControl->selectedObject = projects.front();
        projectControl->selectedObject->isSelected = true;
        projectControl->y = projectControl->selectedObject->y - projectControl->selectedObject->buttonTexture->image->getHeight() * 0.7;
        projectControl->x = -205;
        projectControl->setScrollLimits();
        hasProjects = true;
    }
    isInitialized = true;

    settings = SettingsManager::getConfigSettings();
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    if (!(settings.contains("MenuMusic") && settings["MenuMusic"].is_boolean() && !settings["MenuMusic"].get<bool>())) {
#ifdef __NDS__
        if (!SoundPlayer::isSoundPlaying("gfx/menu/mm_ds.wav")) {
            SoundPlayer::playSound("gfx/menu/mm_ds.wav");
        }
#else
        if (!SoundPlayer::isSoundPlaying("gfx/menu/mm_splash.ogg")) {
            SoundPlayer::playSound("gfx/menu/mm_splash.ogg");
        }
#endif
    }

    if (hasProjects) {
        if (projectControl->selectedObject->isPressed({"a"})) {

            if (projectControl->selectedObject->imageId.find("projectBoxFast") != std::string::npos) {
                // Unpacked sb3
                Unzip::filePath = OS::getScratchFolderLocation() + projectControl->selectedObject->text->getText();
                MenuManager::loadProject();
                return;
            } else if (projectControl->selectedObject->imageId.find("projectBox") != std::string::npos) {
                // normal sb3
                Unzip::filePath = OS::getScratchFolderLocation() + projectControl->selectedObject->text->getText() + ".sb3";
                MenuManager::loadProject();
                return;
            } else {
                // Settings button

                auto it = std::find(projects.begin(), projects.end(), projectControl->selectedObject);

                if (it != projects.end()) {
                    size_t index = std::distance(projects.begin(), it);
                    std::string selectedProject = projects[index - 1]->text->getText();

                    UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

                    ProjectSettings *settings = new ProjectSettings(selectedProject, (std::find(UnzippedFiles.begin(), UnzippedFiles.end(), selectedProject) != UnzippedFiles.end()));
                    MenuManager::changeMenu(settings);
                    return;
                }
            }
        }
        // if (settingsButton->isPressed({"l"})) {
        //     std::string selectedProject = projectControl->selectedObject->text->getText();

        //     UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

        //     ProjectSettings *settings = new ProjectSettings(selectedProject, (std::find(UnzippedFiles.begin(), UnzippedFiles.end(), selectedProject) != UnzippedFiles.end()));
        //     MenuManager::changeMenu(settings);
        //     return;
        // }
    } else {
        if (noProjectsButton->isPressed({"a"})) {
            MenuManager::changeMenu(MenuManager::previousMenu);
            return;
        }
    }

    if (backButton->isPressed({"b", "y"})) {
        MainMenu *main = new MainMenu();
        MenuManager::changeMenu(main);
        return;
    }

    Render::beginFrame(0, 97, 73, 97);
    Render::beginFrame(1, 97, 73, 97);

    for (ButtonObject *project : projects) {
        if (project == nullptr) continue;

        if (projectControl->selectedObject == project)
            project->text->setColor(Math::color(0, 0, 0, 255));
        else
            project->text->setColor(Math::color(32, 36, 41, 255));
    }
    if (hasProjects) {
        projectControl->render();
    } else {
        noProjectsButton->render();
        noProjectsText->render(Render::getWidth() / 2, Render::getHeight() * 0.75);
        noProjectInfo->render(Render::getWidth() / 2, Render::getHeight() * 0.85);
        projectControl->render();
    }
    backButton->render();
    Render::endFrame(false); // dont flush cus projectBoxFast might get freed otherwise
}

void ProjectMenu::cleanup() {
    if (!settings.empty()) {
        settings.clear();
    }

    projectFiles.clear();
    UnzippedFiles.clear();
    for (ButtonObject *button : projects) {
        delete button;
    }
    if (projectControl != nullptr) {
        delete projectControl;
        projectControl = nullptr;
    }
    projects.clear();
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (noProjectsButton != nullptr) {
        delete noProjectsButton;
        noProjectsButton = nullptr;
    }
    isInitialized = false;
}
