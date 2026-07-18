#include "input.hpp"
#include "log.hpp"
#include "meta.hpp"
#include <input.hpp>

void extensions::input::registerAPI(Extension *extension) {
    if (!extension->hasPermission(ExtensionPermission::INPUT)) return;

    extension->luaState["input"] = extension->luaState.create_table();
    extension->luaState["input"][sol::metatable_key] = extension->luaState.create_table();

    extension->luaState["input"][sol::metatable_key][sol::meta_function::index] = [](sol::table t, std::string key) -> sol::object {
        auto luaState = t.lua_state();
        sol::state_view currentLua(luaState);

        if (key == "mouseX") {
            return sol::make_object(luaState, Input::mousePointer.x);
        }
        if (key == "mouseY") {
            return sol::make_object(luaState, Input::mousePointer.y);
        }

        return t.raw_get<sol::object>(key);
    };

    extension->luaState["input"]["mappings"] = extension->luaState.create_table();
    for (const auto &control : Input::inputControls)
        extension->luaState["input"]["mappings"][control.first] = control.second;

    extension->luaState["input"]["devices"] = extension->luaState.create_table();
#ifdef PLATFORM_HAS_CONTROLLER
    static_cast<sol::table>(extension->luaState["input"]["devices"]).add("controller");
#endif
#ifdef PLATFORM_HAS_MOUSE
    static_cast<sol::table>(extension->luaState["input"]["devices"]).add("mouse");
#endif
#ifdef PLATFORM_HAS_KEYBOARD
    static_cast<sol::table>(extension->luaState["input"]["devices"]).add("keyboard");
#endif
#ifdef PLATFORM_HAS_TOUCH
    static_cast<sol::table>(extension->luaState["input"]["devices"]).add("touchscreen");
#endif

    extension->luaState["input"]["keyDown"] = [](std::string key) {
        if (key == "any") return Input::inputKeys.size() > 0;
        return std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key) != Input::inputKeys.end();
    };

    extension->luaState["input"]["buttonDown"] = [](std::string key) {
        if (key == "any") return Input::inputButtons.size() > 0;
        return std::find(Input::inputButtons.begin(), Input::inputButtons.end(), key) != Input::inputButtons.end();
    };

    extension->luaState["input"]["mouseDown"] = [](sol::optional<std::string> button) {
        if (!Input::mousePointer.isPressed) return false;

        if (!button.has_value()) button = "any";

        if (button.value() == "any") return true;
        if (button.value() == "left") return Input::mousePointer.mouseButton == Input::Mouse::LEFT;
        if (button.value() == "middle") return Input::mousePointer.mouseButton == Input::Mouse::MIDDLE;
        if (button.value() == "right") return Input::mousePointer.mouseButton == Input::Mouse::RIGHT;

        Log::logError("Invalid mouse button: " + button.value());
        return false;
    };

    extension->luaState["input"]["getAxis"] = [](std::string joystick, std::string axis) -> float {
        if (joystick != "left" && joystick != "right") {
            Log::logError("Invalid joystick: " + joystick);
            return 0;
        }
        if (axis != "x" && axis != "y") {
            Log::logError("Invalid axis: " + axis);
            return 0;
        }

        float axisValue = 0;

        if (joystick == "left" && axis == "x") axisValue = Input::leftJoystick.first;
        else if (joystick == "left" && axis == "y") axisValue = Input::leftJoystick.second;
        else if (joystick == "right" && axis == "x") axisValue = Input::rightJoystick.first;
        else if (joystick == "right" && axis == "y") axisValue = Input::rightJoystick.second;

        return std::abs(axisValue) > 0.15 ? axisValue : 0;
    };
}
