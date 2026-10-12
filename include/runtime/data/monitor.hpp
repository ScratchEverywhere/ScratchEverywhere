#pragma once
#include <se_export.hpp>
#include <value.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct SE_EXPORT Monitor {
    std::string id;
    std::string mode;
    std::string opcode;
    std::unordered_map<std::string, std::string> parameters;
    std::string spriteName;
    std::string displayName;
    Value value;
    std::vector<Value> list;
    int x;
    int y;
    int width;
    int height;
    int listPage = 0;
    float listScroll = 0.0f;
    float hitboxX = 0, hitboxY = 0, hitboxW = 0, hitboxH = 0;
    bool visible;
    double sliderMin;
    double sliderMax;
    bool isDiscrete;
};
