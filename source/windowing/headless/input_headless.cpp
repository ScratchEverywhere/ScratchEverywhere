#include "input_headless.hpp"
#include <input.hpp>
#include <iostream>
#include <log.hpp>
#include <os.hpp>

InputDevice_Headless::InputDevice_Headless() {}
InputDevice_Headless::~InputDevice_Headless() {}

std::array<int, 2> InputDevice_Headless::getTouchPosition() {
    return {0, 0};
}

void InputDevice_Headless::getInput() {}

std::string InputDevice_Headless::openSoftwareKeyboard(const char *hintText) {
    Log::log(std::string(hintText));
    std::string input;
    std::getline(std::cin, input);
    return input;
}