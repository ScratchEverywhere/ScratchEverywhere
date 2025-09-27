#include "keyboard.hpp"
#include <3ds.h>

static SwkbdState swkbd;
static char mybuf[60];
static SwkbdStatusData swkbdStatus;
static SwkbdLearningData swkbdLearning;
static SwkbdButton button = SWKBD_BUTTON_NONE;
static bool didit = false;

std::string Keyboard::openKeyboard(const char *hintText) {

    didit = true;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetInitialText(&swkbd, mybuf);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Submit", true);
    swkbdSetFeatures(&swkbd, SWKBD_PREDICTIVE_INPUT);
    static bool reload = false;
    swkbdSetStatusData(&swkbd, &swkbdStatus, reload, true);
    swkbdSetLearningData(&swkbd, &swkbdLearning, reload, true);
    reload = true;
    button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));

    if (button != SWKBD_BUTTON_NONE) {
        std::string result = mybuf;
        return result;
    }

    return "";
}
