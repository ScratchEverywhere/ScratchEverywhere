#pragma once
#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_set>
#include <vector>

class Input {
  public:
    struct Mouse {
        int x;
        int y;
        size_t heldFrames;
        bool isPressed;
        bool isMoving;
    };
    static Mouse mousePointer;
    static Sprite *draggingSprite;

    static std::vector<std::string> inputButtons;
    static std::map<std::string, std::string> inputControls;
    static std::vector<std::string> inputBuffer;
    static std::unordered_map<std::string, int> keyHeldDuration;
    static std::unordered_set<std::string> codePressedBlockOpcodes;

    static std::vector<int> getTouchPosition();
    static void getInput();

    static void applyControls(std::string controlsFilePath = "");
    static void buttonPress(std::string button);
    static std::string convertToKey(const Value keyName, const bool uppercaseKeys = false);
    static bool checkSequenceMatch(const std::vector<std::string> sequence);

    static std::string openSoftwareKeyboard(const char *hintText);
};
