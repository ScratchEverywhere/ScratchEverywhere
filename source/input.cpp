#include "input.hpp"
#include <log.hpp>

std::pair<float, float> Input::leftJoystick = {0, 0};
std::pair<float, float> Input::rightJoystick = {0, 0};

std::vector<std::string> Input::inputButtons;
std::vector<std::string> Input::inputKeys;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<Block *> Input::codePressedBlockOpcodes;
Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

int Input::inputViewportX = 0;
int Input::inputViewportY = 0;
int Input::inputViewportW = 0;
int Input::inputViewportH = 0;

void Input::setInputViewport(int x, int y, int w, int h) {
    inputViewportX = x;
    inputViewportY = y;
    inputViewportW = w;
    inputViewportH = h;
}

void Input::clearInputViewport() {
    inputViewportW = 0;
    inputViewportH = 0;
}

bool Input::hasInputViewport() {
    return inputViewportW > 0 && inputViewportH > 0;
}

void Input::applyInputViewportOffset(int &x, int &y) {
    if (!hasInputViewport()) return;
    x -= inputViewportX;
    y -= inputViewportY;
}

void Input::scaleViewportToRenderSpace(int &x, int &y, int renderWidth, int renderHeight) {
    if (!hasInputViewport()) return;
    x = static_cast<int>(x * (static_cast<float>(renderWidth) / inputViewportW));
    y = static_cast<int>(y * (static_cast<float>(renderHeight) / inputViewportH));
}

void *Input::inputWindowHandleOverride = nullptr;

void Input::setInputWindowHandle(void *handle) {
    inputWindowHandleOverride = handle;
}

void Input::clearInputWindowHandle() {
    inputWindowHandleOverride = nullptr;
}

void Input::applyControls(std::string controlsFilePath) {
    Input::inputControls.clear();

    if (controlsFilePath != "" && Scratch::projectType == ProjectType::UNEMBEDDED) {
        // load controls from file
        std::ifstream file(controlsFilePath);
        if (file.is_open()) {
            Log::log("Loading controls from file: " + controlsFilePath);
            nlohmann::json controlsJson;
            file >> controlsJson;

            // Access the "controls" object specifically
            if (controlsJson.contains("controls")) {
                for (auto &[key, value] : controlsJson["controls"].items()) {
                    if (key.empty() || value.empty()) continue;
                    Input::inputControls[value.get<std::string>()] = key;
                    Log::log("Loaded control: " + key + " -> " + value.get<std::string>());
                }
                file.close();
                return;
            } else {
                Log::logWarning("settings file does not contain controls.");
                file.close();
            }
        } else {
            Log::logWarning("Failed to open controls file: " + controlsFilePath);
        }
    }

    // default controls
    size_t arr_size = sizeof(SCRATCH_CONTROLS) / sizeof(SCRATCH_CONTROLS[0]);

    // for ()

    for (size_t i = 0; i < static_cast<int>(SCRATCH_KEY_INDEX::COUNT); i++) {
        Input::inputControls[CONTROLLER_STRINGS[i]] = SCRATCH_CONTROLS[i];
    }
}

void Input::buttonPress(std::string button) {
    Input::inputButtons.push_back(button);
    if (Input::inputControls.find(button) != Input::inputControls.end()) {
        Input::inputKeys.push_back(Input::inputControls[button]);
    }
}

std::string Input::convertToKey(const Value keyName, const bool uppercaseKeys) {
    if (keyName.isDouble()) {
        const double key = keyName.get<double>();
        if (key >= 48 && key <= 90) {
            return std::string(1, std::tolower(static_cast<char>(static_cast<int>(key))));
        }

        switch (static_cast<int>(key)) {
        case 32:
            return "space";
            break;
        case 37:
            return "left arrow";
            break;
        case 38:
            return "up arrow";
            break;
        case 39:
            return "right arrow";
            break;
        case 50:
            return "down arrow";
            break;
        };
    }

    std::string key = keyName.get<std::string>();

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

bool Input::checkSequenceMatch(const std::vector<std::string> sequence) {
    if (inputBuffer.size() >= sequence.size()) {
        std::vector<std::string> slicedBuffer((Input::inputBuffer).end() - sequence.size(), Input::inputBuffer.end());
        for (unsigned int i = 0; i < sequence.size(); i++) {
            if (sequence[i] != slicedBuffer[i]) return false;
        }
        return true;
    }
    return false;
}
