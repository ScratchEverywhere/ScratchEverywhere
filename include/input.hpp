#pragma once
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <se_export.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include <input_strings.hpp>

class SE_EXPORT InputDevice {
  public:
    InputDevice() = default;
    virtual ~InputDevice() = default;

    virtual std::array<int, 2> getTouchPosition() = 0;

    virtual void getInput() = 0;

    virtual std::string openSoftwareKeyboard(const char *hintText) { return ""; };

    virtual void setClipboardText(const std::string &text) {};
};

extern InputDevice *globalInputDevice;

class SE_EXPORT Input {
  public:
    struct Mouse {
        int x;
        int y;
        size_t heldFrames;
        bool isPressed;
        bool isMoving;

        enum {
            LEFT,
            MIDDLE,
            RIGHT
        } mouseButton;
    };
    static Mouse mousePointer;
    static Sprite *draggingSprite;

    static int mouseScrollDelta;

    static std::pair<float, float> leftJoystick;
    static std::pair<float, float> rightJoystick;

    static std::vector<std::string> inputButtons;
    static std::vector<std::string> inputKeys;
    static std::map<std::string, std::string> inputControls;
    static std::vector<std::string> inputBuffer;
    static std::unordered_map<std::string, int> keyHeldDuration;
    static std::unordered_set<Block *> codePressedBlockOpcodes;

    static InputDevice *getGlobalInputDevice();
    static void setGlobalInputDevice(InputDevice *device);
    static void destroyGlobalInputDevice();

    static void applyControls(std::string controlsFilePath = "");

    static int inputViewportX, inputViewportY, inputViewportW, inputViewportH;
    static void setInputViewport(int x, int y, int w, int h);
    static void clearInputViewport();
    static bool hasInputViewport();
    static void applyInputViewportOffset(int &x, int &y);
    static void scaleViewportToRenderSpace(int &x, int &y, int renderWidth, int renderHeight);

    static void *inputWindowHandleOverride;
    static void setInputWindowHandle(void *handle);
    static void clearInputWindowHandle();

    static void buttonPress(std::string button);
    static std::string convertToKey(const Value keyName, const bool uppercaseKeys = false);
    static bool checkSequenceMatch(const std::vector<std::string> sequence);

    // input device specific functions
    // TODO: remove these and rely on getInputDevice to access functions

    static std::array<int, 2> getTouchPosition();

    static void getInput();

    static std::string openSoftwareKeyboard(const char *hintText);

    static void setClipboardText(const std::string &text);
};
