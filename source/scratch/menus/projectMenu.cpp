#include "projectMenu.hpp"
#include "projectSettings.hpp"
#include "unpackMenu.hpp"

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {

    projectControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->needsToBeSelected = false;
    backButton->scale = 1.0;

    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
    UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

    // initialize text and set positions
    int yPosition = 120;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file.substr(0, file.length() - 4), "gfx/menu/projectBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(0, 0, 0, 255));
        project->canBeClicked = false;
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);
        yPosition += 50;
    }
    for (std::string &file : UnzippedFiles) {
        ButtonObject *project = new ButtonObject(file, "gfx/menu/projectBoxFast.png", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(126, 101, 1, 255));
        project->canBeClicked = false;
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);
        yPosition += 50;
    }

    for (size_t i = 0; i < projects.size(); i++) {
        // Check if there's a project above
        if (i > 0) {
            projects[i]->buttonUp = projects[i - 1];
        }

        // Check if there's a project below
        if (i < projects.size() - 1) {
            projects[i]->buttonDown = projects[i + 1];
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
        noProjectInfo->setText("Put Scratch projects in SD Card A:/scratch-gamecube/ !");
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
        projectControl->selectedObject = projects.front();
        projectControl->selectedObject->isSelected = true;
        cameraY = projectControl->selectedObject->y;
        hasProjects = true;
        playButton = new ButtonObject("Play (A)", "gfx/menu/optionBox.svg", 95, 230, "gfx/menu/Ubuntu-Bold");
        settingsButton = new ButtonObject("Settings (L)", "gfx/menu/optionBox.svg", 315, 230, "gfx/menu/Ubuntu-Bold");
        playButton->scale = 0.6;
        settingsButton->scale = 0.6;
        settingsButton->needsToBeSelected = false;
        playButton->needsToBeSelected = false;
    }
    isInitialized = true;
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    float targetY = 0.0f;
    float lerpSpeed = 0.1f;

    if (hasProjects) {
        if (projectControl->selectedObject->isPressed({"a"}) || playButton->isPressed({"a"})) {

            if (projectControl->selectedObject->buttonTexture->image->imageId == "projectBoxFast") {
                // Unpacked sb3
                Unzip::filePath = projectControl->selectedObject->text->getText();
                MenuManager::loadProject();
                return;
            } else {
                // normal sb3
                Unzip::filePath = projectControl->selectedObject->text->getText() + ".sb3";
                MenuManager::loadProject();
                return;
            }
        }
        if (settingsButton->isPressed({"l"})) {
            std::string selectedProject = projectControl->selectedObject->text->getText();

            UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

            ProjectSettings *settings = new ProjectSettings(selectedProject, (std::find(UnzippedFiles.begin(), UnzippedFiles.end(), selectedProject) != UnzippedFiles.end()));
            MenuManager::changeMenu(settings);
            return;
        }
        targetY = projectControl->selectedObject->y;
        lerpSpeed = 0.1f;
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

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    cameraX = 200;
    const double cameraYOffset = 110;

    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);

    for (ButtonObject *project : projects) {
        if (project == nullptr) continue;

        if (projectControl->selectedObject == project)
            project->text->setColor(Math::color(32, 36, 41, 255));
        else
            project->text->setColor(Math::color(0, 0, 0, 255));

        const double xPos = project->x + cameraX;
        const double yPos = project->y - (cameraY - cameraYOffset);

        // Calculate target scale based on distance
        const double distance = abs(project->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));

            // Lerp the scale towards the target scale
            project->scale = project->scale + (targetScale - project->scale) * lerpSpeed;

            project->render(xPos, yPos);

        } else {
            targetScale = 0.0f;
        }
    }
    if (hasProjects) {
        playButton->render();
        settingsButton->render();
        projectControl->render(cameraX, cameraY - cameraYOffset);
    } else {
        noProjectsButton->render();
        noProjectsText->render(Render::getWidth() / 2, Render::getHeight() * 0.75);
        noProjectInfo->render(Render::getWidth() / 2, Render::getHeight() * 0.85);
        projectControl->render();
    }
    backButton->render();
    Render::endFrame();
}

void ProjectMenu::cleanup() {
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
    if (playButton != nullptr) {
        delete playButton;
        playButton = nullptr;
    }
    if (settingsButton != nullptr) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (noProjectsButton != nullptr) {
        delete noProjectsButton;
        noProjectsButton = nullptr;
    }
    if (noProjectsText != nullptr) {
        delete noProjectsText;
        noProjectsText = nullptr;
    }
    if (noProjectInfo != nullptr) {
        delete noProjectInfo;
        noProjectInfo = nullptr;
    }
    isInitialized = false;
}