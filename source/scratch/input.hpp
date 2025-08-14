#pragma once
#include "os.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class Input {
  public:
    struct Mouse {
        int x;
        int y;
        bool isPressed;
        bool isMoving;
    };
    static Mouse mousePointer;

    static std::vector<std::string> inputButtons;
    static std::map<std::string, std::string> inputControls;
    static void applyControls(std::string controlsFilePath = "") {

        if (controlsFilePath != "") {
            // load controls from file
            std::ifstream file(OS::getScratchFolderLocation() + controlsFilePath);
            if (file.is_open()) {
                nlohmann::json controlsJson;
                file >> controlsJson;

                for (auto &[key, value] : controlsJson.items()) {
                    inputControls[value.get<std::string>()] = key;
                }

                file.close();
                return;
            }
        }

        // default controls
        inputControls["dpadUp"] = "u";
        inputControls["dpadDown"] = "h";
        inputControls["dpadLeft"] = "g";
        inputControls["dpadRight"] = "j";
        inputControls["A"] = "a";
        inputControls["B"] = "b";
        inputControls["X"] = "x";
        inputControls["Y"] = "y";
        inputControls["shoulderL"] = "l";
        inputControls["shoulderR"] = "r";
        inputControls["start"] = "1";
        inputControls["back"] = "0";
        inputControls["LeftStickRight"] = "right arrow";
        inputControls["LeftStickLeft"] = "left arrow";
        inputControls["LeftStickDown"] = "down arrow";
        inputControls["LeftStickUp"] = "up arrow";
        inputControls["RightStickRight"] = "5";
        inputControls["RightStickLeft"] = "4";
        inputControls["RightStickDown"] = "3";
        inputControls["RightStickUp"] = "2";
        inputControls["LT"] = "z";
        inputControls["RT"] = "f";
    }

    static void buttonPress(std::string button) {
        if (inputControls.find(button) != inputControls.end()) {
            inputButtons.push_back(inputControls[button]);
        }
    }

    static bool isKeyJustPressed(std::string scratchKey) {
        return (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), scratchKey) != Input::inputButtons.end()) &&
               Input::keyHeldFrames < 2;
    }

    static std::vector<int> getTouchPosition();
    static void getInput();
    static std::string getUsername();
    static int keyHeldFrames;
};