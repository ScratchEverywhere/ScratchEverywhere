#include "../scratch/keyboard.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

/**
 * currently does nothing in the PC and Wii U versions 😁😁
 */
std::string Keyboard::openKeyboard(const char *hintText) {
#ifdef __SWITCH__
    SwkbdConfig kbd;

    Result rc = swkbdCreate(&kbd, 0);
    if (R_SUCCEEDED(rc)) {
        swkbdConfigMakePresetDefault(&kbd);
        swkbdConfigSetOkButtonText(&kbd, "Answer");
        swkbdConfigSetGuideText(&kbd, hintText);

        char outstr[60] = {0};
        rc = swkbdShow(&kbd, outstr, sizeof(outstr));
        swkbdClose(&kbd);
        if (R_SUCCEEDED(rc)) return std::string(outstr);
    }
#endif

    return "";
}
