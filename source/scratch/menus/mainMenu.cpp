#include "mainMenu.hpp"
#include "audio.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "keyboard.hpp"
#include "projectMenu.hpp"
#include "settingsMenu.hpp"
#include <cctype>

Menu::~Menu() = default;

Menu *MenuManager::currentMenu = nullptr;
Menu *MenuManager::previousMenu = nullptr;
int MenuManager::isProjectLoaded = 0;

void MenuManager::changeMenu(Menu *menu) {
    if (currentMenu != nullptr)
        currentMenu->cleanup();

    if (previousMenu != nullptr && previousMenu != menu) {
        delete previousMenu;
        previousMenu = nullptr;
    }

    if (menu != nullptr) {
        previousMenu = currentMenu;
        currentMenu = menu;
        if (!currentMenu->isInitialized)
            currentMenu->init();
    } else {
        currentMenu = nullptr;
    }
}

void MenuManager::render() {
    if (currentMenu && currentMenu != nullptr) {
        currentMenu->render();
    }
}

bool MenuManager::loadProject() {
    if (currentMenu != nullptr) {
        currentMenu->cleanup();
        delete currentMenu;
        currentMenu = nullptr;
    }
    if (previousMenu != nullptr) {
        delete previousMenu;
        previousMenu = nullptr;
    }

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();

    if (!Unzip::load()) {
        Log::logWarning("Could not load project. closing app.");
        isProjectLoaded = -1;
        return false;
    }
    isProjectLoaded = 1;
    return true;
}

MainMenu::MainMenu() {
    init();
}
MainMenu::~MainMenu() {
    cleanup();
}

void MainMenu::init() {

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;

    logo = new MenuImage("gfx/menu/logo.png");
    logo->x = 200;
    logoStartTime.start();

    versionNumber = createTextObject("Beta Build 25", 0, 0, "gfx/menu/Ubuntu-Bold");
    versionNumber->setCenterAligned(false);
    versionNumber->setScale(0.75);

    splashText = createTextObject(Unzip::getSplashText(), 0, 0, "gfx/menu/Ubuntu-Bold");
    splashText->setCenterAligned(true);
    splashText->setColor(Math::color(243, 154, 37, 255));
    if (splashText->getSize()[0] > logo->image->getWidth() * 0.95) {
        splashText->scale = (float)logo->image->getWidth() / (splashText->getSize()[0] * 1.15);
    }

    loadButton = new ButtonObject("", "gfx/menu/play.svg", 100, 180, "gfx/menu/Ubuntu-Bold");
    loadButton->isSelected = true;
    settingsButton = new ButtonObject("", "gfx/menu/settings.svg", 300, 180, "gfx/menu/Ubuntu-Bold");

    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    loadButton->buttonRight = settingsButton;
    settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    mainMenuControl->buttonObjects.push_back(settingsButton);
    isInitialized = true;
}

void MainMenu::render() {

    Input::getInput();
    mainMenuControl->input();

    if (loadButton->isPressed()) {
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    // begin frame
    Render::beginFrame(0, 117, 77, 117);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f;
    float splashZoom = std::sin(elapsed * 0.0085f) * 0.005f;
    splashText->scale += splashZoom;
    logo->y = 75 + bobbingOffset;
    logo->render();
    versionNumber->render(Render::getWidth() * 0.01, Render::getHeight() * 0.935);
    splashText->render(logo->renderX, logo->renderY + 85);

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    if (settingsButton->isPressed()) {
        SettingsMenu *settingsMenu = new SettingsMenu();
        MenuManager::changeMenu(settingsMenu);
        return;
    }

    loadButton->render();
    settingsButton->render();
    mainMenuControl->render();

    Render::endFrame();
}
void MainMenu::cleanup() {

    if (logo) {
        delete logo;
        logo = nullptr;
    }
    if (loadButton) {
        delete loadButton;
        loadButton = nullptr;
    }
    if (settingsButton) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (mainMenuControl) {
        delete mainMenuControl;
        mainMenuControl = nullptr;
    }
    if (versionNumber) {
        delete versionNumber;
        versionNumber = nullptr;
    }
    if (splashText) {
        delete splashText;
        splashText = nullptr;
    }
    isInitialized = false;
}