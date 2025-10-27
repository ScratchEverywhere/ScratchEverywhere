#include <optional>
#include <string>

namespace extensions::input {
bool keyDown(std::string key);
bool buttonDown(std::string button);
bool mouseDown(std::optional<std::string> button);
double getAxis(std::string joystick, std::string axis);
} // namespace extensions::input
