#include <input.hpp>
#include <iostream>
#include <os.hpp>

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