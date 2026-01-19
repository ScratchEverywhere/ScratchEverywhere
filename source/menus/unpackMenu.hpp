#pragma once
#include "mainMenu.hpp"

class UnpackMenu : public Menu {
  public:
    ControlObject *settingsControl = nullptr;

    std::unique_ptr<TextObject> infoText = nullptr;
    std::unique_ptr<TextObject> descText = nullptr;

    bool shouldGoBack = false;

    std::string filename;

    UnpackMenu();
    ~UnpackMenu();

    static void addToJsonArray(const std::string &filePath, const std::string &value);
    static std::vector<std::string> getJsonArray(const std::string &filePath);
    static void removeFromJsonArray(const std::string &filePath, const std::string &value);

    void init() override;
    void render() override;
    void cleanup() override;
};