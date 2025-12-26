#include <iostream>
#include <keyboard.hpp>
#include <os.hpp>

std::string SoftwareKeyboard::openKeyboard(const char *hintText) {
    Log::log(std::string(hintText));
    std::string input;
    std::getline(std::cin, input);
    return input;
}
