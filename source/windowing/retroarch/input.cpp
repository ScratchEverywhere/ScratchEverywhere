#include "window.hpp"
#include <blockExecutor.hpp>
#include <input.hpp>
#include <render.hpp>
#include <string>
#include <text.hpp>
#include <vector>

std::vector<int> Input::getTouchPosition() {
    return {0, 0};
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = 0;

    if (!inputButtons.empty()) {
        inputButtons.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}
