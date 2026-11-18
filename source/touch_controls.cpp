#include "touch_controls.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <memory>
#include <menuManager.hpp>
#include <nlohmann/json.hpp>
#include <renderer.hpp>
#include <runtime.hpp>
#include <set>
#include <string>
#include <text.hpp>
#include <unzip.hpp>
#include <vector>

#if defined(PLATFORM_HAS_TOUCH) && !defined(PLATFORM_HAS_MOUSE)

namespace {

using KeyList = std::vector<std::string>;

struct KeyButton {
    std::string control;
    std::string label;
    float x = 0;
    float y = 0;
    float size = 0;
    bool pressed = false;
};

struct DPad {
    bool active = false;
    float x = 0;
    float y = 0;
    float size = 0;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct Joystick {
    bool active = false;
    float x = 0;
    float y = 0;
    float radius = 0;
    float headRadius = 0;
    float headX = 0;
    float headY = 0;
    bool engaged = false;
};

struct PauseButton {
    float x = 0;
    float y = 0;
    float size = 0;
};

struct Layout {
    bool built = false;
    bool visible = false;
    bool showPause = false;
    DPad dpad;
    Joystick joystick;
    PauseButton pause;
    std::vector<KeyButton> buttons;
};

TouchControls::Mode mode = TouchControls::Mode::Auto;
bool initialized = false;
bool active = false;
bool pauseAvailable = false;
bool pauseTouching = false;
bool pauseRequested = false;
bool joystickWasEngaged = false;

bool usesAllArrows = false;
bool usesAllWasd = false;
KeyList rightControls;

Layout layout;

std::shared_ptr<Image> buttonImage;
std::shared_ptr<Image> buttonPressedImage;
std::shared_ptr<Image> dpadImage;
std::shared_ptr<Image> dpadPressedImage;
std::shared_ptr<Image> joystickBgImage;
std::shared_ptr<Image> joystickHeadImage;

std::unique_ptr<TextObject> labelText;
std::unique_ptr<TextObject> pauseText;

constexpr float kArrowDeadzone = 0.28f;
constexpr float kJoystickDeadzone = 0.28f;

// Layout sizes are in density-independent (dp) pixels so that controls
// have a comfortable size on both lodpi and hidpi screens.
constexpr float kBaseSizeFraction = 0.2025f; // fraction of the shorter screen edge
constexpr float kMinBaseSizeDp = 48.6f;
constexpr float kMaxBaseSizeDp = 172.8f;
constexpr float kMinMarginDp = 8.0f;
constexpr float kMinGapDp = 6.0f;
constexpr float kMarginFraction = 0.025f;
constexpr float kGapFraction = 0.10f;

float displayDensity() {
    const float density = Render::getPixelDensity();
    if (!std::isfinite(density) || density <= 0.0f) return 1.0f;
    return std::clamp(density, 0.5f, 8.0f);
}

bool insideCircle(float px, float py, float cx, float cy, float radius) {
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy <= radius * radius;
}

std::string labelForControl(const std::string &control) {
    if (control == "up arrow") return "↑";
    if (control == "down arrow") return "↓";
    if (control == "left arrow") return "←";
    if (control == "right arrow") return "→";
    if (control == "space") return "SPACE";
    if (control == "enter") return "ENTER";
    if (control == "backspace") return "BKSP";
    if (control == "delete") return "DEL";
    if (control == "escape") return "ESC";
    if (control == "shift") return "SHIFT";
    if (control == "control") return "CTRL";
    if (control == "tab") return "TAB";
    if (control == "page up") return "PGUP";
    if (control == "page down") return "PGDN";
    if (control == "home") return "HOME";
    if (control == "end") return "END";
    if (control == "insert") return "INS";
    if (control == "caps lock") return "CAPS";
    if (control == "any") return "ANY";

    std::string label = control;
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (label.size() > 6) label.resize(6);
    return label;
}

void loadImages() {
    auto load = [](const char *path) -> std::shared_ptr<Image> {
        auto result = createImageFromFile(path, false);
        if (!result.has_value()) {
            Log::logWarning(std::string("Failed to load on-screen control image: ") + path + " (" + result.error() + ")");
            return nullptr;
        }
        return result.value();
    };

    buttonImage = load("gfx/ingame/oscbutton.svg");
    buttonPressedImage = load("gfx/ingame/oscbutton_pressed.svg");
    dpadImage = load("gfx/ingame/dpad.svg");
    dpadPressedImage = load("gfx/ingame/dpad_pressed.svg");
    joystickBgImage = load("gfx/ingame/joystick_bg.svg");
    joystickHeadImage = load("gfx/ingame/joystick_head.svg");
}

void loadText() {
    labelText = createTextObject("", 0, 0, "gfx/menu/Ubuntu-Bold");
    labelText->setColor(Math::color(255, 255, 255, 255));
    labelText->setCenterAligned(true);

    pauseText = createTextObject("II", 0, 0, "gfx/menu/Ubuntu-Bold");
    pauseText->setColor(Math::color(255, 255, 255, 255));
    pauseText->setCenterAligned(true);
}

void readProjectControls() {
    usesAllArrows = false;
    usesAllWasd = false;
    rightControls.clear();

    const std::vector<std::string> controls = Scratch::getUsedControls();
    const std::set<std::string> used(controls.begin(), controls.end());

    usesAllArrows = used.count("up arrow") && used.count("down arrow") || used.count("left arrow") && used.count("right arrow");
    usesAllWasd = used.count("w") && used.count("s") || used.count("a") && used.count("d");

    for (const std::string &control : controls) {
        if (control.empty() || control == "any") continue;
        if (usesAllArrows && (control == "up arrow" || control == "down arrow" || control == "left arrow" || control == "right arrow")) continue;
        if (usesAllWasd && (control == "w" || control == "a" || control == "s" || control == "d")) continue;
        if (std::find(rightControls.begin(), rightControls.end(), control) != rightControls.end()) continue;
        rightControls.push_back(control);
    }
}

void readMode() {
    mode = TouchControls::Mode::Auto;
    const nlohmann::json setting = Unzip::getSetting("onScreenControls");
    if (setting.is_string()) {
        mode = TouchControls::modeFromString(setting.get<std::string>());
    }
}

void buildLayout() {
    layout = Layout{};
    const int width = Render::getWidth();
    const int height = Render::getHeight();
    if (width <= 0 || height <= 0) return;

    layout.built = true;

    const float density = displayDensity();
    const float shortSide = static_cast<float>(std::min(width, height));
    const float logicalShortSide = shortSide / density;
    const float base = std::clamp(logicalShortSide * kBaseSizeFraction, kMinBaseSizeDp, kMaxBaseSizeDp) * density;
    const float margin = std::max(kMinMarginDp * density, shortSide * kMarginFraction);
    const float gap = std::max(kMinGapDp * density, base * kGapFraction);

    layout.dpad.active = usesAllArrows;
    if (layout.dpad.active) {
        layout.dpad.size = base * 2.65f;
        layout.dpad.x = margin + layout.dpad.size * 0.5f;
        layout.dpad.y = static_cast<float>(height) - margin - layout.dpad.size * 0.5f;
    }

    layout.joystick.active = usesAllWasd;
    if (layout.joystick.active) {
        const float joystickSize = base * 1.75f;
        layout.joystick.radius = joystickSize * 0.5f;
        layout.joystick.headRadius = joystickSize * 0.22f;
        layout.joystick.x = margin + layout.joystick.radius;
        if (layout.dpad.active) {
            layout.joystick.y = layout.dpad.y - layout.dpad.size * 0.5f - gap - layout.joystick.radius;
        } else {
            layout.joystick.y = static_cast<float>(height) - margin - layout.joystick.radius;
        }
        layout.joystick.headX = layout.joystick.x;
        layout.joystick.headY = layout.joystick.y;
    }

    layout.showPause = pauseAvailable;
    layout.pause.size = base * 0.90f;
    layout.pause.x = static_cast<float>(width) - margin - layout.pause.size * 0.5f;
    layout.pause.y = margin + layout.pause.size * 0.5f;

    float leftEdge = margin;
    if (layout.dpad.active) leftEdge = std::max(leftEdge, layout.dpad.x + layout.dpad.size * 0.5f);
    if (layout.joystick.active) leftEdge = std::max(leftEdge, layout.joystick.x + layout.joystick.radius);

    const float rightEdge = static_cast<float>(width) - margin;
    const float availableWidth = std::max(base, rightEdge - leftEdge - gap * 2.0f);
    const float topReserve = layout.showPause ? (layout.pause.size + gap) : 0.0f;
    const float availableHeight = std::max(base, static_cast<float>(height) - margin * 2.0f - topReserve);

    const int count = static_cast<int>(rightControls.size());
    if (count > 0) {
        int columns = 1;
        if (count > 9) columns = 4;
        else if (count > 4) columns = 3;
        else if (count > 1) columns = 2;
        const int rows = (count + columns - 1) / columns;

        const float widthLimit = (availableWidth - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        const float heightLimit = (availableHeight - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
        const float cell = std::max(12.0f, std::min(base, std::min(widthLimit, heightLimit)));

        const float gridWidth = static_cast<float>(columns) * cell + static_cast<float>(columns - 1) * gap;
        const float gridHeight = static_cast<float>(rows) * cell + static_cast<float>(rows - 1) * gap;
        const float startX = rightEdge - gridWidth + cell * 0.5f;
        const float startY = static_cast<float>(height) - margin - gridHeight + cell * 0.5f;

        for (int i = 0; i < count; i++) {
            const int row = i / columns;
            const int column = i % columns;
            KeyButton button;
            button.control = rightControls[static_cast<size_t>(i)];
            button.label = labelForControl(button.control);
            button.size = cell;
            button.x = startX + static_cast<float>(column) * (cell + gap);
            button.y = startY + static_cast<float>(row) * (cell + gap);
            layout.buttons.push_back(std::move(button));
        }
    }
}

void clearHeldState() {
    for (KeyButton &button : layout.buttons)
        button.pressed = false;
    layout.dpad.up = layout.dpad.down = layout.dpad.left = layout.dpad.right = false;
    layout.joystick.engaged = false;
    layout.joystick.headX = layout.joystick.x;
    layout.joystick.headY = layout.joystick.y;
    pauseTouching = false;
    if (joystickWasEngaged) {
        Input::leftJoystick = {0, 0};
        joystickWasEngaged = false;
    }
}

void addKey(KeyList &keys, const std::string &key) {
    if (key.empty() || key == "any") return;
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
}

void pushKeys(const KeyList &keys) {
    for (const std::string &key : keys) {
        if (std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key) == Input::inputKeys.end()) {
            Input::inputKeys.push_back(key);
        }
    }
    if (!keys.empty() && std::find(Input::inputKeys.begin(), Input::inputKeys.end(), "any") == Input::inputKeys.end()) {
        Input::inputKeys.push_back("any");
    }
}

void renderImage(const std::shared_ptr<Image> &image, float x, float y, float size, float opacity = 1.0f) {
    if (!image || size <= 0) return;
    const float imageWidth = static_cast<float>(image->getWidth());
    if (imageWidth <= 0) return;

    ImageRenderParams params;
    params.x = x;
    params.y = y;
    params.scale = size / imageWidth;
    params.centered = true;
    params.opacity = opacity;
    image->render(params);
}

float renderLabel(TextObject *text, const std::string &label, float x, float y, float maxWidth, float maxHeight, int color = Math::color(255, 255, 255, 255)) {
    if (!text) return 1.0f;
    text->setText(label);
    text->setColor(color);
    text->setScale(1.0f);

    const std::vector<float> size = text->getSize();
    if (size[0] <= 0.0f || size[1] <= 0.0f) return 1.0f;

    const float scale = std::clamp(std::min(maxWidth / size[0], maxHeight / size[1]), 0.15f, 2.2f);
    text->setScale(scale);
    text->render(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
    return scale;
}

void drawDPad() {
    if (!layout.dpad.active) return;

    const DPad &dpad = layout.dpad;
    renderImage(dpadImage, dpad.x, dpad.y, dpad.size, 0.95f);

    const float offset = dpad.size * 0.285f;
    const float highlightSize = dpad.size * 0.34f;
    const float labelSize = dpad.size * 0.22f;

    struct Direction {
        bool pressed;
        float x;
        float y;
        const char *label;
    } directions[] = {
        {dpad.up, dpad.x, dpad.y - offset, "·"},
        {dpad.down, dpad.x, dpad.y + offset, "·"},
        {dpad.left, dpad.x - offset, dpad.y, "·"},
        {dpad.right, dpad.x + offset, dpad.y, "·"},
    };

    for (const Direction &direction : directions) {
        if (direction.pressed) renderImage(dpadPressedImage, direction.x, direction.y, highlightSize, 1.0f);
        renderLabel(labelText.get(), direction.label, direction.x, direction.y, labelSize, labelSize);
    }
}

void drawJoystick() {
    if (!layout.joystick.active) return;

    const Joystick &joystick = layout.joystick;
    renderImage(joystickBgImage, joystick.x, joystick.y, joystick.radius * 2.0f, 0.90f);
    renderImage(joystickHeadImage, joystick.headX, joystick.headY, joystick.headRadius * 2.0f, 1.0f);
}

void drawButtons() {
    for (const KeyButton &button : layout.buttons) {
        renderImage(button.pressed ? buttonPressedImage : buttonImage, button.x, button.y, button.size, 1.0f);
        renderLabel(labelText.get(), button.label, button.x, button.y, button.size * 0.62f, button.size * 0.44f);
    }
}

} // namespace

namespace TouchControls {

Mode modeFromString(const std::string &value) {
    if (value == "on") return Mode::On;
    if (value == "off") return Mode::Off;
    return Mode::Auto;
}

const char *modeToString(Mode value) {
    switch (value) {
    case Mode::On:
        return "on";
    case Mode::Off:
        return "off";
    case Mode::Auto:
        break;
    }
    return "auto";
}

void init() {
    cleanup();
    active = true;
    readMode();
    readProjectControls();
    loadImages();
    loadText();

    pauseAvailable = false;
#ifdef ENABLE_MENU
    pauseAvailable = Scratch::projectType == ProjectType::UNEMBEDDED ||
                     (Scratch::projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD);
#endif

    buildLayout();
    Log::log("On-screen controls initialized (mode " + std::string(modeToString(mode)) + ", " +
             std::to_string(rightControls.size()) + " right-side buttons)");
}

void cleanup() {
    active = false;
    initialized = false;
    pauseAvailable = false;
    pauseTouching = false;
    pauseRequested = false;
    joystickWasEngaged = false;
    usesAllArrows = false;
    usesAllWasd = false;
    rightControls.clear();
    layout = Layout{};

    labelText.reset();
    pauseText.reset();

    buttonImage.reset();
    buttonPressedImage.reset();
    dpadImage.reset();
    dpadPressedImage.reset();
    joystickBgImage.reset();
    joystickHeadImage.reset();
}

void update() {
    if (!active) return;

#ifdef ENABLE_MENU
    if (MenuManager::currentMenu == MenuId::PauseMenu) {
        clearHeldState();
        return;
    }
#endif

    buildLayout();
    initialized = true;

    const bool gamepadConnected = Input::hasConnectedGamepad();
    layout.visible = mode == Mode::On || (mode == Mode::Auto && !gamepadConnected);

    if (!layout.visible) {
        clearHeldState();
        return;
    }

    for (KeyButton &button : layout.buttons)
        button.pressed = false;
    layout.dpad.up = layout.dpad.down = layout.dpad.left = layout.dpad.right = false;
    layout.joystick.engaged = false;
    layout.joystick.headX = layout.joystick.x;
    layout.joystick.headY = layout.joystick.y;

    const std::vector<InputTouch> touches = Input::getTouches();
    bool pauseNow = false;
    bool joystickTouched = false;
    KeyList keys;

    for (const InputTouch &touch : touches) {
        const float x = static_cast<float>(touch.x);
        const float y = static_cast<float>(touch.y);

        if (layout.showPause && insideCircle(x, y, layout.pause.x, layout.pause.y, layout.pause.size * 0.5f)) {
            pauseNow = true;
            continue;
        }

        if (layout.dpad.active) {
            const float dx = x - layout.dpad.x;
            const float dy = y - layout.dpad.y;
            const float radius = layout.dpad.size * 0.5f;
            if (dx * dx + dy * dy <= radius * radius) {
                const float threshold = radius * kArrowDeadzone;
                if (dy < -threshold) layout.dpad.up = true;
                if (dy > threshold) layout.dpad.down = true;
                if (dx < -threshold) layout.dpad.left = true;
                if (dx > threshold) layout.dpad.right = true;
                continue;
            }
        }

        if (layout.joystick.active) {
            const float dx = x - layout.joystick.x;
            const float dy = y - layout.joystick.y;
            const float activationRadius = layout.joystick.radius * 1.30f;
            if (dx * dx + dy * dy <= activationRadius * activationRadius) {
                const float maxDistance = layout.joystick.radius * 0.72f;
                const float distance = std::sqrt(dx * dx + dy * dy);
                float clampedDx = dx;
                float clampedDy = dy;
                if (distance > maxDistance && distance > 0.0f) {
                    clampedDx = dx / distance * maxDistance;
                    clampedDy = dy / distance * maxDistance;
                }

                layout.joystick.headX = layout.joystick.x + clampedDx;
                layout.joystick.headY = layout.joystick.y + clampedDy;
                layout.joystick.engaged = true;
                joystickTouched = true;

                const float axisX = std::clamp(clampedDx / maxDistance, -1.0f, 1.0f);
                const float axisY = std::clamp(clampedDy / maxDistance, -1.0f, 1.0f);
                Input::leftJoystick = {axisX, axisY};
                joystickWasEngaged = true;

                if (axisX < -kJoystickDeadzone) addKey(keys, "a");
                if (axisX > kJoystickDeadzone) addKey(keys, "d");
                if (axisY < -kJoystickDeadzone) addKey(keys, "w");
                if (axisY > kJoystickDeadzone) addKey(keys, "s");
                continue;
            }
        }

        for (KeyButton &button : layout.buttons) {
            if (insideCircle(x, y, button.x, button.y, button.size * 0.54f)) {
                button.pressed = true;
            }
        }
    }

    if (!joystickTouched && joystickWasEngaged) {
        Input::leftJoystick = {0, 0};
        joystickWasEngaged = false;
    }

    if (pauseNow && !pauseTouching) pauseRequested = true;
    pauseTouching = pauseNow;

    if (layout.dpad.up) addKey(keys, "up arrow");
    if (layout.dpad.down) addKey(keys, "down arrow");
    if (layout.dpad.left) addKey(keys, "left arrow");
    if (layout.dpad.right) addKey(keys, "right arrow");

    for (const KeyButton &button : layout.buttons) {
        if (button.pressed) addKey(keys, button.control);
    }

    pushKeys(keys);
}

void render() {
    if (!active || !layout.built || !layout.visible) return;

    if (layout.showPause) {
        renderImage(buttonImage, layout.pause.x, layout.pause.y, layout.pause.size, 0.95f);
        renderLabel(pauseText.get(), "II", layout.pause.x, layout.pause.y, layout.pause.size * 0.40f, layout.pause.size * 0.40f);
    }

    drawDPad();
    drawJoystick();
    drawButtons();
}

bool isPointOverControls(int x, int y) {
    if (!active || !layout.built || !layout.visible) return false;

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);

    if (layout.showPause && insideCircle(fx, fy, layout.pause.x, layout.pause.y, layout.pause.size * 0.55f)) return true;

    if (layout.dpad.active) {
        const float dx = fx - layout.dpad.x;
        const float dy = fy - layout.dpad.y;
        const float radius = layout.dpad.size * 0.5f;
        if (dx * dx + dy * dy <= radius * radius) return true;
    }

    if (layout.joystick.active) {
        const float dx = fx - layout.joystick.x;
        const float dy = fy - layout.joystick.y;
        const float radius = layout.joystick.radius * 1.30f;
        if (dx * dx + dy * dy <= radius * radius) return true;
    }

    for (const KeyButton &button : layout.buttons) {
        if (insideCircle(fx, fy, button.x, button.y, button.size * 0.55f)) return true;
    }

    return false;
}

bool consumePauseRequest() {
    const bool requested = pauseRequested;
    pauseRequested = false;
    return requested;
}

bool isVisible() {
    return active && layout.visible;
}

} // namespace TouchControls

#else // touch controls are not compiled on platforms with a mouse

namespace TouchControls {

Mode modeFromString(const std::string &value) {
    if (value == "on") return Mode::On;
    if (value == "off") return Mode::Off;
    return Mode::Auto;
}

const char *modeToString(Mode value) {
    switch (value) {
    case Mode::On:
        return "on";
    case Mode::Off:
        return "off";
    case Mode::Auto:
        break;
    }
    return "auto";
}

void init() {}
void cleanup() {}
void update() {}
void render() {}
bool isPointOverControls(int, int) { return false; }
bool consumePauseRequest() { return false; }
bool isVisible() { return false; }

} // namespace TouchControls

#endif
