#include "log.hpp"
#include "window_win32.hpp"
#include <algorithm>
#include <blockExecutor.hpp>
#include <cctype>
#include <cstddef>
#include <input.hpp>
#include <map>
#include <render.hpp>
#include <string>
#include <types.hpp>
#include <vector>

/**
 * I do not know how to do joystick/touch in modern Windows. Sorry!
 * (nishi)
 */

std::array<int, 2> Input::getTouchPosition() {
    std::array<int, 2> pos = {0, 0};
    POINT p;

    GetCursorPos(&p); /* appearantly this function can fail? but WINSTA_READATTRIBUTES is given to process by default */
    ScreenToClient((HWND)globalWindow->getHandle(), &p);
    pos[0] = p.x;
    pos[1] = p.y;

    Input::applyInputViewportOffset(pos[0], pos[1]);
    Input::scaleViewportToRenderSpace(pos[0], pos[1], Render::getWidth(), Render::getHeight());
    return pos;
}

/* this misses input events. isn't that bad? but that's how SDL2 did so i am not going to care... */
void Input::getInput() {
    inputButtons.clear();
    inputKeys.clear();
    mousePointer.isPressed = false;

#ifdef PLATFORM_HAS_KEYBOARD
    BYTE keys[256];

    GetKeyboardState(keys);

    for (int sc = 0; sc < 256; ++sc) {
        if (!(keys[sc] & 0x80)) continue;

        char c[2];
        WORD ch;

        c[0] = '?';
        c[1] = 0;

        if (ToAscii(sc, 0, keys, &ch, 0) > 0) {
            c[0] = tolower(LOBYTE(ch));
        }

        std::string keyName = c;

        if (sc == VK_UP) keyName = "up arrow";
        else if (sc == VK_DOWN) keyName = "down arrow";
        else if (sc == VK_LEFT) keyName = "left arrow";
        else if (sc == VK_RIGHT) keyName = "right arrow";
        else if (sc == VK_LSHIFT || sc == VK_RSHIFT) keyName = "shift";
        else if (sc == VK_LCONTROL || sc == VK_RCONTROL) keyName = "control";

        inputKeys.push_back(keyName);
    }
#endif

    if (!inputKeys.empty()) inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_MOUSE
    std::array<int, 2> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    int l = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
    int m = GetAsyncKeyState(VK_MBUTTON) & 0x8000;
    int r = GetAsyncKeyState(VK_RBUTTON) & 0x8000;

    if (l || m) {
        mousePointer.isPressed = true;
    }

    if (r) {
        mousePointer.mouseButton = Mouse::RIGHT;
    } else if (m) {
        mousePointer.mouseButton = Mouse::MIDDLE;
    } else {
        mousePointer.mouseButton = Mouse::LEFT;
    }
#endif

    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}
