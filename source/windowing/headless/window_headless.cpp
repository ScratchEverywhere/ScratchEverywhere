#include "window_headless.hpp"
#include "input_headless.hpp"

WindowHeadless::WindowHeadless(int width, int height, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_Headless);
}

void WindowHeadless::cleanup() {}
bool WindowHeadless::shouldClose() { return false; }
void WindowHeadless::pollEvents() {}
void WindowHeadless::swapBuffers() {}
void WindowHeadless::resize(int width, int height) {}
int WindowHeadless::getWidth() const { return 0; }
int WindowHeadless::getHeight() const { return 0; }
float WindowHeadless::getPixelDensity() const { return 1.0f; }
void *WindowHeadless::getHandle() { return nullptr; }
