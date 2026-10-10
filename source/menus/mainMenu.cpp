#include "mainMenu.hpp"
#include "projectMenu.hpp"
#include "settings.hpp"
#include "settingsMenu.hpp"
#include "unpackMenu.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <cctype>
#include <cmath>
#include <image.hpp>
#include <log.hpp>
#include <parser.hpp>
#include <translation.hpp>
#ifdef __XBOX__
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

#include <version_config.h>

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
    std::string answer = Input::openSoftwareKeyboard(TranslationManager::getTranslation("ui.headless.projectPrompt").c_str());

    for (const std::string &ext : {".sb3", ".sb2", ".sb"}) {
        if (answer.size() >= ext.size() &&
            answer.compare(answer.size() - ext.size(), ext.size(), ext) == 0) {
            answer = answer.substr(0, answer.size() - ext.size());
            break;
        }
    }

    ProjectFormat format;
    std::string resolved = Unzip::resolveZipProjectPath(OS::getScratchFolderLocation() + answer, format);
    if (!resolved.empty()) {
        Unzip::filePath = resolved;
    } else {
        Unzip::filePath = OS::getScratchFolderLocation() + answer;
        Unzip::unpackedFormatHint = UnpackMenu::getUnpackedFormat(OS::getScratchFolderLocation() + "UnpackedGames.json", answer);
    }

    MenuManager::loadProject();
    return;
#endif

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;

#if defined(__XBOX__)
    Log::log("MainMenu::init: loading logo.png...");
#endif
    logo = new MenuImage("gfx/menu/logo.png");
    logo->x = 200;
    logoStartTime.start();

    std::string versionStr;
    switch (SE_VERSION_TYPE) {
    case SE_VERSION_TYPE_RELEASE:
        versionStr = TranslationManager::getTranslation("version.prefix.release") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_BETA:
        versionStr = TranslationManager::getTranslation("version.prefix.beta") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_ALPHA:
        versionStr = TranslationManager::getTranslation("version.prefix.alpha") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_RELEASE_CANDIDATE:
        versionStr = TranslationManager::getTranslation("version.prefix.releaseCanidate") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_NIGHTLY:
        versionStr = TranslationManager::getTranslation("version.prefix.nightly") + " " + SE_VERSION_DISPLAY;
        break;
    default:
        versionStr = TranslationManager::getTranslation("version.prefix.dev");
        break;
    }
#if defined(__XBOX__)
    Log::log("MainMenu::init: creating version text...");
#endif
    versionNumber = createTextObject(versionStr, 0, 0, "gfx/menu/Ubuntu-Bold");
    versionNumber->setCenterAligned(false);
    versionNumber->setScale(0.75);

#if defined(__XBOX__)
    Log::log("MainMenu::init: creating splash text...");
#endif
    splashText = createTextObject(TranslationManager::getSplashText(), 0, 0, "gfx/menu/Ubuntu-Bold");
    splashText->setCenterAligned(true);
    splashText->setColor(Math::color(255, 255, 255, 128));
    if (logo && logo->image && splashText->getSize()[0] > logo->image->getWidth() * 0.95) {
        splashTextOriginalScale = (float)logo->image->getWidth() / (splashText->getSize()[0] * 1.15);
        splashText->scale = splashTextOriginalScale;
    } else {
        splashTextOriginalScale = splashText->scale;
    }
#if defined(__XBOX__)
    Log::log("MainMenu::init: loading loadButton play.svg...");
#endif
    loadButton = new ButtonObject("", "gfx/menu/play.svg", 100, 180, "gfx/menu/Ubuntu-Bold");
    loadButton->isSelected = true;
#if defined(__XBOX__)
    Log::log("MainMenu::init: loading settingsButton settings.svg...");
#endif
    settingsButton = new ButtonObject("", "gfx/menu/settings.svg", 300, 180, "gfx/menu/Ubuntu-Bold");

    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    loadButton->buttonRight = settingsButton;
    settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    mainMenuControl->buttonObjects.push_back(settingsButton);
    isInitialized = true;
#if defined(__XBOX__)
    Log::log("MainMenu::init: finished successfully!");
#endif

    settings = SettingsManager::getConfigSettings();
}

void MainMenu::render() {
#if defined(__XBOX__)
    static int mmFrameCount = 0;
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: frame=" + std::to_string(mmFrameCount) + " entering...");
    }
#endif
    Input::getInput();
    mainMenuControl->input();

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: checking MenuMusic setting...");
    }
#endif
    if (!(settings != nullptr && settings.contains("MenuMusic") && settings["MenuMusic"].is_boolean() && !settings["MenuMusic"].get<bool>())) {
#ifdef __NDS__
        if (!Mixer::isSoundPlaying("gfx/nds/mm_ds.wav")) {
            SoundStream *strm = new SoundStream("gfx/nds/mm_ds.wav");
            if (strm->error.has_value()) {
#if defined(__XBOX__)
                Log::logError(strm->error.value());
#else
                Log::log(strm->error.value());
#endif
                delete strm;
            } else
                Mixer::setAutoClean("gfx/nds/mm_ds.wav", true);
        }
#else
#if defined(__XBOX__)
        if (mmFrameCount < 5) {
            Log::log("MainMenu::render: checking if mm_splash.ogg is playing...");
        }
#endif
        if (!Mixer::isSoundPlaying("gfx/menu/mm_splash.ogg")) {
#if defined(__XBOX__)
            Log::log("MainMenu::render: mm_splash.ogg not playing; allocating new SoundStream(\"gfx/menu/mm_splash.ogg\")...");
#endif
            SoundStream *strm = new SoundStream("gfx/menu/mm_splash.ogg");
            if (strm->error.has_value()) {
#if defined(__XBOX__)
                Log::logError("MainMenu::render: SoundStream error: " + strm->error.value());
#else
                Log::log(strm->error.value());
#endif
                delete strm;
            } else {
#if defined(__XBOX__)
                Log::log("MainMenu::render: SoundStream created successfully; setting auto clean...");
#endif
                Mixer::setAutoClean("gfx/menu/mm_splash.ogg", true);
            }
        }
#endif
    }
#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: audio check completed");
    }
#endif

    if (loadButton->isPressed()) {
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: calling Render::beginFrame(0)...");
    }
#endif
    Render::beginFrame(0, 117, 77, 117);

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: Render::beginFrame(0) done, rendering logo...");
    }
#endif
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

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: calling Render::beginFrame(1)...");
    }
#endif
    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    if (settingsButton->isPressed()) {
        SettingsMenu *settingsMenu = new SettingsMenu();
        MenuManager::changeMenu(settingsMenu);
        return;
    }

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: rendering mainMenuControl...");
    }
#endif
    mainMenuControl->render();

#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: calling Render::endFrame()...");
    }
#endif
    Render::endFrame();
#if defined(__XBOX__)
    if (mmFrameCount < 5) {
        Log::log("MainMenu::render: frame=" + std::to_string(mmFrameCount) + " completed successfully");
    }
    mmFrameCount++;
#endif
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
