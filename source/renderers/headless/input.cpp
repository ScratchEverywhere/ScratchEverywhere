#include <input.hpp>

// Static member initialization
Input::Mouse Input::mousePointer = {0, 0, 0, false, false};
Sprite *Input::draggingSprite = nullptr;
std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;

std::vector<int> Input::getTouchPosition() {
    return {0, 0};
}

void Input::getInput() {
}

std::string Input::getUsername() {
    return "";
}
