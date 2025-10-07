#include "controlsMenu.hpp"

ControlsMenu::ControlsMenu(std::string projPath) {
    projectPath = projPath;
    init();
}

ControlsMenu::~ControlsMenu() {
    cleanup();
}

void ControlsMenu::init() {

    Unzip::filePath = projectPath + ".sb3";
    if (!Unzip::load()) {
        Log::logError("Failed to load project for ControlsMenu.");
        toExit = true;
        return;
    }
    Unzip::filePath = "";

    // get controls
    std::vector<std::string> controls;

    for (auto &sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            std::string buttonCheck;
            if (block.opcode == "sensing_keypressed") {

                // stolen code from sensing.cpp

                auto inputFind = block.parsedInputs->find("KEY_OPTION");
                // if no variable block is in the input
                if (inputFind->second.inputType == ParsedInput::LITERAL) {
                    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
                    if (Scratch::getFieldValue(*inputBlock, "KEY_OPTION") != "")
                        buttonCheck = Scratch::getFieldValue(*inputBlock, "KEY_OPTION");
                } else {
                    buttonCheck = Scratch::getInputValue(block, "KEY_OPTION", sprite).asString();
                }

            } else if (block.opcode == "event_whenkeypressed") {
                buttonCheck = Scratch::getFieldValue(block, "KEY_OPTION");
                ;
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
    applyButton = new ButtonObject("Apply (Y)", "gfx/menu/optionBox.svg", 200, 230, "gfx/menu/Ubuntu-Bold");
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
        ButtonObject *controlButton = new ButtonObject(control, "gfx/menu/optionBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        controlButton->text->setColor(Math::color(255, 255, 255, 255));
        controlButton->scale = 0.6;
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
        cameraY = settingsControl->selectedObject->y;
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
        applyControls();
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    if (settingsControl->selectedObject->isPressed()) {
        Input::keyHeldFrames = -999;

        // wait till A isnt pressed
        while (!Input::inputButtons.empty() && Render::appShouldRun()) {
            Input::getInput();
        }

        while (Input::keyHeldFrames < 2 && Render::appShouldRun()) {
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

    // Smooth camera movement to follow selected control
    const float targetY = settingsControl->selectedObject->y;
    const float lerpSpeed = 0.1f;

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    const int cameraX = 200;
    const double cameraYOffset = 110;

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

        // Position with camera offset
        const double xPos = controlButton.button->x + cameraX;
        const double yPos = controlButton.button->y - (cameraY - cameraYOffset);

        // Scale based on distance to selected
        const double distance = abs(controlButton.button->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));
        } else {
            targetScale = 0.0f;
        }

        // Smooth scaling
        controlButton.button->scale = controlButton.button->scale + (targetScale - controlButton.button->scale) * lerpSpeed;

        controlButton.button->render(xPos, yPos);
    }

    // Render UI elements
    settingsControl->render(cameraX, cameraY - cameraYOffset);
    backButton->render();
    applyButton->render();

    Render::endFrame();
}

void ControlsMenu::applyControls() {
    // Build the file path
    std::string folderPath = OS::getScratchFolderLocation() + projectPath;
    std::string filePath = folderPath + ".sb3" + ".json";

    // Make sure parent directories exist
    try {
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    } catch (const std::filesystem::filesystem_error &e) {
        Log::logError("Failed to create directories: " + std::string(e.what()));
        return;
    }

    // Create a JSON object to hold control mappings
    nlohmann::json json;
    json["controls"] = nlohmann::json::object();

    // Save each control in the form: "ControlName": "MappedKey"
    for (const auto &c : controlButtons) {
        json["controls"][c.control] = c.controlValue;
    }

    // Write JSON to file (overwrite if exists)
    std::ofstream file(filePath);
    if (!file) {
        Log::logError("Failed to create JSON file: " + filePath);
        return;
    }
    file << json.dump(2);
    file.close();

    Log::log("Controls saved to: " + filePath);
}

void ControlsMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    for (key button : controlButtons) {
        delete button.button;
    }
    controlButtons.clear();
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