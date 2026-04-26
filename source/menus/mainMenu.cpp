#include "mainMenu.hpp"
#include "projectMenu.hpp"
#include "settings.hpp"
#include "settingsMenu.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <cctype>
#include <cmath>
#include <image.hpp>
#include <parser.hpp>

#include <nlohmann/json.hpp>

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
    cleanup();
    Mixer::cleanupAudio();

    if (!Unzip::load()) {
        Log::logWarning("Could not load project: " + Unzip::filePath + ". closing app.");
        isProjectLoaded = -1;
        return false;
    }
    isProjectLoaded = 1;
    return true;
}

void MenuManager::cleanup() {
    if (currentMenu != nullptr) {
        currentMenu->cleanup();
        delete currentMenu;
        currentMenu = nullptr;
    }
    if (previousMenu != nullptr) {
        delete previousMenu;
        previousMenu = nullptr;
    }
}

MainMenu::MainMenu() {
    init();
}
MainMenu::~MainMenu() {
    cleanup();
}

void MainMenu::init() {
#if defined(RENDERER_HEADLESS) || !defined(ENABLE_SVG) || !defined(ENABLE_BITMAP)
    // let the user type what project they want to open
    std::string answer = Input::openSoftwareKeyboard("Please type what project you want to open.");

    const std::string ext = ".sb3";
    if (answer.size() >= ext.size() &&
        answer.compare(answer.size() - ext.size(), ext.size(), ext) == 0) {
        answer = answer.substr(0, answer.size() - ext.size());
    }

    Unzip::filePath = OS::getScratchFolderLocation() + answer + ".sb3";

    MenuManager::loadProject();
    return;
#endif

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;

    logo = new MenuImage("gfx/menu/logo.png");
    logo->x = 200;
    logoStartTime.start();

    versionNumber = createTextObject("Beta Build 40", 0, 0, "gfx/menu/Ubuntu-Bold");
    versionNumber->setCenterAligned(false);
    versionNumber->setScale(0.75);

    splashText = createTextObject(Unzip::getSplashText(), 0, 0, "gfx/menu/Ubuntu-Bold");
    splashText->setCenterAligned(true);
    splashText->setColor(Math::color(243, 154, 37, 255));
    if (splashText->getSize()[0] > logo->image->getWidth() * 0.95) {
        splashTextOriginalScale = (float)logo->image->getWidth() / (splashText->getSize()[0] * 1.15);
        splashText->scale = splashTextOriginalScale;
    } else {
        splashTextOriginalScale = splashText->scale;
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

    settings = SettingsManager::getConfigSettings();
}

void MainMenu::render() {
    Input::getInput();
    mainMenuControl->input();

    if (!(settings != nullptr && settings.contains("MenuMusic") && settings["MenuMusic"].is_boolean() && !settings["MenuMusic"].get<bool>())) {
#ifdef __NDS__
        if (!Mixer::isSoundPlaying("gfx/nds/mm_ds.wav")) {
            SoundStream *strm = new SoundStream("gfx/nds/mm_ds.wav");
            if (strm->error.has_value()) {
                Log::log(strm->error.value());
                delete strm;
            } else
                Mixer::setAutoClean("gfx/nds/mm_ds.wav", true);
        }
#else
        if (!Mixer::isSoundPlaying("gfx/menu/mm_splash.ogg")) {
            SoundStream *strm = new SoundStream("gfx/menu/mm_splash.ogg");
            if (strm->error.has_value()) {
                Log::log(strm->error.value());
                delete strm;
            } else
                Mixer::setAutoClean("gfx/menu/mm_splash.ogg", true);
        }
#endif
    }

    if (loadButton->isPressed()) {
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    Render::beginFrame(0, 117, 77, 117);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    // fmod to prevent precision issues with large elapsed times
    float bobbingOffset = std::sin(std::fmod(elapsed * 0.0025f, 2.0f * M_PI)) * 5.0f;
    float splashZoom = std::sin(std::fmod(elapsed * 0.0085f, 2.0f * M_PI)) * 0.05f;
    splashText->scale = splashTextOriginalScale + splashZoom;
    logo->y = 75 + bobbingOffset;
    logo->render();
    versionNumber->render(Render::getWidth() * 0.01, Render::getHeight() * 0.900);
    splashText->render(logo->renderX, logo->renderY + ((logo->image->getHeight() * 0.7) * MenuObject::getScaleFactor()));

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    if (settingsButton->isPressed()) {
        SettingsMenu *settingsMenu = new SettingsMenu();
        MenuManager::changeMenu(settingsMenu);
        return;
    }

    mainMenuControl->render();

    Render::endFrame();
}
void MainMenu::cleanup() {
    if (!settings.empty()) {
        settings.clear();
    }

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
    isInitialized = false;
}
