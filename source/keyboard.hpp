#pragma once
#include <string>

class Keyboard {
  public:
    /**
     * Opens the Software Keyboard so the user can type in what they want.
     * @param hintText
     * @return An `std::string` of the contents of what the user typed.
     */
    std::string openKeyboard(const char *hintText);
};
