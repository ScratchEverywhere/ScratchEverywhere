#include "log.hpp"
#include "window_win32.hpp"
#include <algorithm>
#include <blockExecutor.hpp>
#include <cctype>
#include <cstddef>
#include <input_win32.hpp>
#include <map>
#include <renderer.hpp>
#include <string>
#include <types.hpp>
#include <vector>

/**
 * I do not know how to do joystick/touch in modern Windows. Sorry!
 * (nishi)
 */

InputDevice_Win32::InputDevice_Win32() {}
InputDevice_Win32::~InputDevice_Win32() {}

std::array<int, 2> InputDevice_Win32::getTouchPosition() {
    std::array<int, 2> pos = {0, 0};
    POINT p;

    GetCursorPos(&p); /* appearantly this function can fail? but WINSTA_READATTRIBUTES is given to process by default */
    ScreenToClient((HWND)Render::getGlobalRenderer()->window->getHandle(), &p);
    pos[0] = p.x;
    pos[1] = p.y;

    Input::applyInputViewportOffset(pos[0], pos[1]);
    Input::scaleViewportToRenderSpace(pos[0], pos[1], Render::getWidth(), Render::getHeight());
    return pos;
}

/* this misses input events. isn't that bad? but that's how SDL2 did so i am not going to care... */
void InputDevice_Win32::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;

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
        else if (sc == VK_SPACE) keyName = "space";
        else if (sc == VK_RETURN) keyName = "enter";

        Input::inputKeys.push_back(keyName);
    }
#endif

    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_MOUSE
    std::array<int, 2> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    Input::mousePointer.x = coords.first;
    Input::mousePointer.y = coords.second;

    int l = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
    int m = GetAsyncKeyState(VK_MBUTTON) & 0x8000;
    int r = GetAsyncKeyState(VK_RBUTTON) & 0x8000;

    if (l || m) {
        Input::mousePointer.isPressed = true;
    }

    if (r) {
        Input::mousePointer.mouseButton = Mouse::RIGHT;
    } else if (m) {
        Input::mousePointer.mouseButton = Mouse::MIDDLE;
    } else {
        Input::mousePointer.mouseButton = Mouse::LEFT;
    }
#endif

    BlockExecutor::doSpriteClicking();
}

void InputDevice_Win32::setClipboardText(const std::string &text) {
    if (!OpenClipboard((HWND)Render::getGlobalRenderer()->window->getHandle())) return;

    EmptyClipboard();

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (mem) {
        memcpy(GlobalLock(mem), text.c_str(), text.size() + 1);
        GlobalUnlock(mem);
        SetClipboardData(CF_TEXT, mem);
    }

    CloseClipboard();
}
