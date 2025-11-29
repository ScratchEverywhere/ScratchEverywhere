#include "../scratch/keyboard.hpp"
#include "../scratch/os.hpp"
#include <iostream>

std::string Keyboard::openKeyboard(const char *hintText) {
    Log::log(std::string(hintText));
    std::string input;
    std::getline(std::cin, input);
    return input;
}