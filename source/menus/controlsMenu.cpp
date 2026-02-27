#include "controlsMenu.hpp"
#include <settings.hpp>

ControlsMenu::ControlsMenu(std::string projPath) {
    projectPath = projPath;
    init();
}

ControlsMenu::~ControlsMenu() {
    cleanup();
}

void ControlsMenu::init() {

    Unzip::filePath = OS::getScratchFolderLocation() + projectPath + ".sb3";
    if (!Unzip::load()) {
        Log::logError("Failed to load project for ControlsMenu.");
        OS::toExit = true;
        return;
    }
    Unzip::filePath = "";

    // get controls
    std::vector<std::string> controls;

    for (auto &sprite : Scratch::sprites) {
        for (auto &[id, block] : sprite->blocks) {
            std::string buttonCheck;
            if (block.opcode == "sensing_keypressed") {
                buttonCheck = Input::convertToKey(Scratch::getInputValue(block, "KEY_OPTION", sprite));
            } else if (block.opcode == "event_whenkeypressed") {
                buttonCheck = Input::convertToKey(Value(Scratch::getFieldValue(block, "KEY_OPTION")));
            } else if (block.opcode == "makeymakey_whenMakeyKeyPressed") {
                buttonCheck = Input::convertToKey(Scratch::getInputValue(block, "KEY", sprite), true);
            } else if (block.opcode == "makeymakey_whenCodePressed") {
                std::string input = Scratch::getInputValue(block, "SEQUENCE", sprite).asString();
                size_t start = 0;
                size_t end;
                while ((end = input.find(' ', start)) != std::string::npos) {
                    buttonCheck = input.substr(start, end - start);
                    if (buttonCheck != "" && std::find(controls.begin(), controls.end(), buttonCheck) == controls.end()) {
                        Log::log("Found new control: " + buttonCheck);
                        controls.push_back(buttonCheck);
                    }
                    start = end + 1;
                }
                buttonCheck = input.substr(start);
                if (buttonCheck != "" && std::find(controls.begin(), controls.end(), buttonCheck) == controls.end()) {
                    Log::log("Found new control: " + buttonCheck);
                    controls.push_back(buttonCheck);
                }
                continue;
            } else continue;
            if (buttonCheck != "" && std::find(controls.begin(), controls.end(), buttonCheck) == controls.end()) {
                Log::log("Found new control: " + buttonCheck);
                controls.push_back(buttonCheck);
            }
        }
    }

    Scratch::cleanupScratchProject();
    Render::renderMode = Render::BOTH_SCREENS;

    settingsControl = new ControlObject();
    settingsControl->selectedObject = nullptr;
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    applyButton = new ButtonObject("Apply (Y)", "gfx/menu/optionBox.svg", 340, 230, "gfx/menu/Ubuntu-Bold", true);
    applyButton->scale = 0.6;
    applyButton->needsToBeSelected = false;
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    if (controls.empty()) {
        Log::logWarning("No controls found in project");
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    double yPosition = 100;
    for (auto &control : controls) {
        key newControl;
        ButtonObject *controlButton = new ButtonObject(control, "gfx/menu/projectBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold", true);
        controlButton->text->setColor(Math::color(255, 255, 255, 255));
        controlButton->scale = 1.0;
        controlButton->y -= controlButton->text->getSize()[1] / 2;
        if (controlButton->text->getSize()[0] > controlButton->buttonTexture->image->getWidth() * 0.3) {
            float scale = (float)controlButton->buttonTexture->image->getWidth() / (controlButton->text->getSize()[0] * 3);
            controlButton->textScale = scale;
        }
        controlButton->canBeClicked = false;
        newControl.button = controlButton;
        newControl.control = control;

        for (const auto &pair : Input::inputControls) {
            if (pair.second == newControl.control) {
                newControl.controlValue = pair.first;
                break;
            }
        }

        controlButtons.push_back(newControl);
        settingsControl->buttonObjects.push_back(controlButton);
        yPosition += 50;
    }
    if (!controls.empty()) {
        settingsControl->selectedObject = controlButtons.front().button;
        settingsControl->selectedObject->isSelected = true;
        settingsControl->y = settingsControl->selectedObject->y - settingsControl->selectedObject->buttonTexture->image->getHeight() * 0.7;
        settingsControl->x = -205;
        settingsControl->enableScrolling = true;
        settingsControl->setScrollLimits();
    }

    // link buttons
    for (size_t i = 0; i < controlButtons.size(); i++) {
        if (i > 0) {
            controlButtons[i].button->buttonUp = controlButtons[i - 1].button;
        }
        if (i < controlButtons.size() - 1) {
            controlButtons[i].button->buttonDown = controlButtons[i + 1].button;
        }
    }

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;
    isInitialized = true;
}

void ControlsMenu::render() {
    Input::getInput();
    settingsControl->input();

    if (backButton->isPressed({"b"})) {
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }
    if (applyButton->isPressed({"y"})) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        for (const auto &c : controlButtons) {
            settings["controls"][c.control] = c.controlValue;
        }
        SettingsManager::saveProjectSettings(settings, projectPath);
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    if (settingsControl->selectedObject->isPressed()) {

        // wait till A isnt pressed
        while (!Input::inputButtons.empty() && Render::appShouldRun()) {
            Input::getInput();
        }

        while (Input::inputButtons.empty() && Render::appShouldRun()) {
            Input::getInput();
        }
        if (!Input::inputButtons.empty()) {

            // remove "any" first
            auto it = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "any");
            if (it != Input::inputButtons.end()) {
                Input::inputButtons.erase(it);
            }

            std::string key = Input::inputButtons.back();
            for (const auto &pair : Input::inputControls) {
                if (pair.second == key) {
                    // Update the control value
                    for (auto &newControl : controlButtons) {
                        if (newControl.button == settingsControl->selectedObject) {
                            newControl.controlValue = pair.first;
                            Log::log("Updated control: " + newControl.control + " -> " + newControl.controlValue);
                            break;
                        }
                    }
                    break;
                }
            }
        } else {
            Log::logWarning("No input detected for control assignment.");
        }
    }

    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);

    for (key &controlButton : controlButtons) {
        if (controlButton.button == nullptr) continue;

        // Update button text
        controlButton.button->text->setText(
            controlButton.control + " = " + controlButton.controlValue);

        // Highlight selected
        if (settingsControl->selectedObject == controlButton.button)
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));
        else
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));
    }

    // Render UI elements
    settingsControl->render();
    backButton->render();
    applyButton->render();

    Render::endFrame();
}

void ControlsMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (applyButton != nullptr) {
        delete applyButton;
        applyButton = nullptr;
    }
    // Render::beginFrame(0, 181, 165, 111);
    // Render::beginFrame(1, 181, 165, 111);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}
