#include "receiveMenu.hpp"
#include "input.hpp"
#include "mainMenu.hpp"
#include "render.hpp"
#include "translation.hpp"

ReceiveMenu::ReceiveMenu() {
    init();
}

ReceiveMenu::~ReceiveMenu() {
    cleanup();
}

void ReceiveMenu::init() {
    ProjectReceiver::init();
    shortCode = ProjectReceiver::getShortCode();

    receiveControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->needsToBeSelected = false;

    infoText = createTextObject(TranslationManager::getTranslation("ui.projects.receive.info"), 0, 0, "gfx/menu/Ubuntu-Bold");
    infoText->setCenterAligned(true);

    shortCodeText = createTextObject(shortCode, 0, 0, "gfx/menu/Ubuntu-Bold");
    shortCodeText->setCenterAligned(true);
    shortCodeText->setScale(2.0);

    statusText = createTextObject(TranslationManager::getTranslation("ui.projects.receive.status"), 0, 0, "gfx/menu/Ubuntu-Bold");
    statusText->setCenterAligned(true);
    statusText->setColor(Math::color(0, 150, 0, 255));

    isInitialized = true;
}

void ReceiveMenu::render() {
    Input::getInput();
    receiveControl->input();

    ProjectReceiver::update();

    if (backButton->isPressed({"b", "y"})) {
        MenuManager::changeMenu(new MainMenu());
        return;
    }

    Render::beginFrame(0, 71, 107, 115);
    Render::beginFrame(1, 71, 107, 115);

    infoText->render(Render::getWidth() / 2, Render::getHeight() * 0.3);
    shortCodeText->render(Render::getWidth() / 2, Render::getHeight() * 0.5);
    statusText->render(Render::getWidth() / 2, Render::getHeight() * 0.7);

    backButton->render();
    Render::endFrame();
}

void ReceiveMenu::cleanup() {
    ProjectReceiver::deinit();
    if (receiveControl) {
        delete receiveControl;
        receiveControl = nullptr;
    }
    if (backButton) {
        delete backButton;
        backButton = nullptr;
    }
    isInitialized = false;
}
