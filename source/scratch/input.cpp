#include "input.hpp"

void Input::applyControls(std::string controlsFilePath) {
    Input::inputControls.clear();

    if (controlsFilePath != "" && projectType == UNEMBEDDED) {
        // load controls from file
        std::ifstream file(controlsFilePath);
        if (file.is_open()) {
            Log::log("Loading controls from file: " + controlsFilePath);
            nlohmann::json controlsJson;
            file >> controlsJson;

            // Access the "controls" object specifically
            if (controlsJson.contains("controls")) {
                for (auto &[key, value] : controlsJson["controls"].items()) {
                    Input::inputControls[value.get<std::string>()] = key;
                    Log::log("Loaded control: " + key + " -> " + value.get<std::string>());
                }
            }
            file.close();
            return;
        } else {
            Log::logWarning("Failed to open controls file: " + controlsFilePath);
        }
    }

    // default controls
    Input::inputControls["dpadUp"] = "u";
    Input::inputControls["dpadDown"] = "h";
    Input::inputControls["dpadLeft"] = "g";
    Input::inputControls["dpadRight"] = "j";
    Input::inputControls["A"] = "a";
    Input::inputControls["B"] = "b";
    Input::inputControls["X"] = "x";
    Input::inputControls["Y"] = "y";
    Input::inputControls["shoulderL"] = "l";
    Input::inputControls["shoulderR"] = "r";
    Input::inputControls["start"] = "1";
    Input::inputControls["back"] = "0";
    Input::inputControls["LeftStickRight"] = "right arrow";
    Input::inputControls["LeftStickLeft"] = "left arrow";
    Input::inputControls["LeftStickDown"] = "down arrow";
    Input::inputControls["LeftStickUp"] = "up arrow";
    Input::inputControls["LeftStickPressed"] = "c";
    Input::inputControls["RightStickRight"] = "5";
    Input::inputControls["RightStickLeft"] = "4";
    Input::inputControls["RightStickDown"] = "3";
    Input::inputControls["RightStickUp"] = "2";
    Input::inputControls["RightStickPressed"] = "v";
    Input::inputControls["LT"] = "z";
    Input::inputControls["RT"] = "f";
}

void Input::buttonPress(std::string button) {
    if (Input::inputControls.find(button) != Input::inputControls.end()) {
        Input::inputButtons.push_back(Input::inputControls[button]);
    }
}

void Input::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : sprites) {
            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                if (Input::mousePointer.heldFrames < 2 && isColliding("mouse", sprite)) {

                    // run all "when this sprite clicked" blocks in the sprite
                    hasClicked = true;
                    for (auto &[id, data] : sprite->blocks) {
                        if (data.opcode == "event_whenthisspriteclicked") {
                            executor.runBlock(data, sprite);
                        }
                    }
                }
            }
            // start dragging a sprite
            if (Input::draggingSprite == nullptr && Input::mousePointer.heldFrames < 2 && sprite->draggable && isColliding("mouse", sprite)) {
                Input::draggingSprite = sprite;
            }
            if (hasClicked) break;
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    // move a dragging sprite
    if (Input::draggingSprite != nullptr) {
        if (Input::mousePointer.heldFrames == 0) {
            Input::draggingSprite = nullptr;
            return;
        }
        Input::draggingSprite->xPosition = Input::mousePointer.x - (Input::draggingSprite->spriteWidth / 2);
        Input::draggingSprite->yPosition = Input::mousePointer.y + (Input::draggingSprite->spriteHeight / 2);
    }
}

std::string Input::convertToKey(const Value keyName, const bool uppercaseKeys) {
    if (keyName.isDouble()) {
        if (keyName.asDouble() >= 48 && keyName.asDouble() <= 90) {
            return std::string(1, std::tolower(static_cast<char>(static_cast<int>(keyName.asDouble()))));
        } else if (keyName.asDouble() == 32.0) {
            return "space";
        } else if (keyName.asDouble() == 37.0) {
            return "left arrow";
        } else if (keyName.asDouble() == 38.0) {
            return "up arrow";
        } else if (keyName.asDouble() == 39.0) {
            return "right arrow";
        } else if (keyName.asDouble() == 50.0) {
            return "down arrow";
        }
    }
    
    std::string key = keyName.asString();

    if (uppercaseKeys) {
        if (key == "SPACE") return "space";
        if (key == "LEFT") return "left arrow";
        if (key == "RIGHT") return "right arrow";
        if (key == "UP") return "up arrow";
        if (key == "DOWN") return "down arrow";
    }

    if (key == "space" || key == "left arrow" || key == "up arrow" || key == "right arrow" || key == "down arrow" || key == "enter" || key == "any") {
        return key;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (key.length() > 0) {
        key = key.substr(0, 1);
    }

    return key;
}

void Input::executeKeyHats() {
    for (const auto& key : keyHeldDuration) {
        if (std::find(inputButtons.begin(), inputButtons.end(), key.first) == inputButtons.end()) {
            keyHeldDuration[key.first] = 0;
        } else {
            keyHeldDuration[key.first]++;
        }
    }

    for (std::string key : inputButtons) {
        if (keyHeldDuration.find(key) == keyHeldDuration.end()) keyHeldDuration[key] = 1;
    }

    for (std::string key : inputButtons) {
        if (key != "any" && keyHeldDuration[key] == 1) {
            codePressedBlockOpcodes.clear();
            std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
            std::transform(addKey.begin(), addKey.end(), addKey.begin(), ::tolower);
            inputBuffer.push_back(addKey);
            if (inputBuffer.size() == 101) inputBuffer.erase(inputBuffer.begin());
        }
    }

    std::vector<Sprite *> sprToRun = sprites;
    for (Sprite *currentSprite : sprToRun) {
        for (auto &[id, data] : currentSprite->blocks) {
            if (data.opcode == "event_whenkeypressed") {
                std::string key = Scratch::getFieldValue(data, "KEY_OPTION");
                if (keyHeldDuration.find(key) != keyHeldDuration.end() && (keyHeldDuration.find(key)->second == 1 || keyHeldDuration.find(key)->second > 13))
                executor.runBlock(data, currentSprite);
            } else if (data.opcode == "makeymakey_whenMakeyKeyPressed") {
                std::string key = convertToKey(Scratch::getInputValue(data, "KEY", currentSprite), true);
                if (keyHeldDuration.find(key) != keyHeldDuration.end() && keyHeldDuration.find(key)->second > 0)
                executor.runBlock(data, currentSprite);
            }
        }
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

bool Input::checkSequenceMatch(const std::vector<std::string> sequence) {
    if (inputBuffer.size() >= sequence.size()) {
        std::vector<std::string> slicedBuffer((Input::inputBuffer).end() - sequence.size(), Input::inputBuffer.end());
        for (int i = 0; i < sequence.size(); i++) {
            if (sequence[i] != slicedBuffer[i]) return false;
        }
        return true;
    }
    return false;
}
