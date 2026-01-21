#include <input.hpp>
#include <iostream>
#include <os.hpp>

Input::Mouse Input::mousePointer = {0, 0, 0, false, false};
Sprite *Input::draggingSprite = nullptr;
std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

std::vector<int> Input::getTouchPosition() {
    return {0, 0};
}

void Input::getInput() {
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    Log::log(std::string(hintText));
    std::string input;
    std::getline(std::cin, input);
    return input;
}