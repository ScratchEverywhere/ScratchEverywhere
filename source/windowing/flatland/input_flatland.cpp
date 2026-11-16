#include "input_flatland.hpp"
#include "flatland_backend.hpp"
#include <algorithm>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <os.hpp>
#include <runtime.hpp>

InputDevice_Flatland::InputDevice_Flatland() {}
InputDevice_Flatland::~InputDevice_Flatland() {}

std::array<int, 2> InputDevice_Flatland::getTouchPosition() {
    auto [x, y] = FlatlandBackend::get().cursorPosition();
    return {static_cast<int>(x), static_cast<int>(y)};
}

// TODO: Add a way to remap.
void InputDevice_Flatland::getInput() {
    FlatlandBackend &backend = FlatlandBackend::get();

    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;

    const FlatlandInputEvents events = backend.takeInputEvents();
    for (const std::string &key : events.keys) {
        auto held = std::find_if(heldPulses.begin(), heldPulses.end(), [&](const auto &pulse) { return pulse.first == key; });
        if (held != heldPulses.end()) held->second = pulseFrames;
        else heldPulses.emplace_back(key, pulseFrames);
    }
    if (events.leftClick) leftClickFrames = pulseFrames;
    if (events.rightClick) rightClickFrames = pulseFrames;

    const flatland::InputState movement = backend.currentMovement();
    if (movement.forward) Input::inputKeys.push_back("up arrow");
    if (movement.backward) Input::inputKeys.push_back("down arrow");
    if (movement.left) Input::inputKeys.push_back("left arrow");
    if (movement.right) Input::inputKeys.push_back("right arrow");
    if (movement.jump) Input::inputKeys.push_back("space");
    if (movement.sneak) Input::inputKeys.push_back("shift");

    for (auto &pulse : heldPulses) {
        Input::inputKeys.push_back(pulse.first);
        pulse.second--;
    }
    heldPulses.erase(std::remove_if(heldPulses.begin(), heldPulses.end(), [](const auto &pulse) { return pulse.second <= 0; }), heldPulses.end());

    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();

    auto [cursorX, cursorY] = backend.cursorPosition();
    Input::mousePointer.x = static_cast<int>(cursorX - Scratch::projectWidth * 0.5);
    Input::mousePointer.y = static_cast<int>(Scratch::projectHeight * 0.5 - cursorY);

    if (leftClickFrames > 0 || rightClickFrames > 0) {
        Input::mousePointer.isPressed = true;
        Input::mousePointer.mouseButton = rightClickFrames > 0 && leftClickFrames <= 0 ? Input::Mouse::RIGHT : Input::Mouse::LEFT;
        leftClickFrames--;
        rightClickFrames--;
    } else {
        Input::mousePointer.mouseButton = Input::Mouse::LEFT;
    }

    BlockExecutor::doSpriteClicking();
}

std::string InputDevice_Flatland::openSoftwareKeyboard(const char *hintText) {
    return FlatlandBackend::get().ask(hintText);
}
