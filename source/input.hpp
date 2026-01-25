#pragma once
#include "menus/menuManager.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_set>
#include <vector>

class Input {
  public:
    struct Mouse {
        int x;
        int y;
        size_t heldFrames;
        bool isPressed;
        bool isMoving;
    };
    static Mouse mousePointer;
    static Sprite *draggingSprite;

    static std::vector<std::string> inputButtons;
    static std::vector<std::string> inputKeys;
    static std::map<std::string, std::string> inputControls;
    static std::vector<std::string> inputBuffer;
    static std::array<float, 2> scrollDelta;
    static std::unordered_map<std::string, int> keyHeldDuration;
    static std::unordered_set<std::string> codePressedBlockOpcodes;

    static std::vector<int> getTouchPosition();
    static void getInput(MenuManager *menuManager = nullptr);

    static void applyControls(std::string controlsFilePath = "") {
        Input::inputControls.clear();

        if (controlsFilePath != "" && Scratch::projectType == UNEMBEDDED) {
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

    static void buttonPress(std::string button) {
        if (Input::inputControls.find(button) != Input::inputControls.end()) {
            Input::inputButtons.push_back(Input::inputControls[button]);
        }
    }

    static std::string convertToKey(const Value keyName, const bool uppercaseKeys = false) {
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

    static bool checkSequenceMatch(const std::vector<std::string> sequence) {
        if (inputBuffer.size() >= sequence.size()) {
            std::vector<std::string> slicedBuffer((Input::inputBuffer).end() - sequence.size(), Input::inputBuffer.end());
            for (unsigned int i = 0; i < sequence.size(); i++) {
                if (sequence[i] != slicedBuffer[i]) return false;
            }
            return true;
        }
        return false;
    }

    static std::string openSoftwareKeyboard(const char *hintText);

    static bool isButtonJustPressed(const std::string &button) {
        return Input::keyHeldDuration.find(Input::inputControls[button]) != Input::keyHeldDuration.end() && Input::keyHeldDuration[Input::inputControls[button]] == 1;
    }
};
