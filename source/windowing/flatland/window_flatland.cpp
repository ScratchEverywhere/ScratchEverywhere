#include "window_flatland.hpp"
#include "flatland_backend.hpp"
#include "input_flatland.hpp"
#include <input.hpp>

WindowFlatland::WindowFlatland(int width, int height, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_Flatland);
    FlatlandBackend::get().start();
}

void WindowFlatland::cleanup() {
    FlatlandBackend::get().stop();
}

bool WindowFlatland::shouldClose() { return false; }
void WindowFlatland::pollEvents() {}
void WindowFlatland::swapBuffers() {}
void WindowFlatland::resize(int width, int height) {}
int WindowFlatland::getWidth() const { return 0; }
int WindowFlatland::getHeight() const { return 0; }
float WindowFlatland::getPixelDensity() const { return 1.0f; }
void *WindowFlatland::getHandle() { return nullptr; }
