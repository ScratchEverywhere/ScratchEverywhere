#pragma once
#include "mainMenu.hpp"
#include <parser.hpp>
#include <se_export.hpp>

class SE_EXPORT UnpackMenu : public Menu {
  public:
    ControlObject *settingsControl = nullptr;

    std::unique_ptr<TextObject> infoText = nullptr;
    std::unique_ptr<TextObject> descText = nullptr;

    bool shouldGoBack = false;

    std::string filename;

    UnpackMenu();
    ~UnpackMenu();

    static void addToJsonArray(const std::string &filePath, const std::string &value, ProjectFormat format = ProjectFormat::SB3);
    static std::vector<std::string> getJsonArray(const std::string &filePath);
    static void removeFromJsonArray(const std::string &filePath, const std::string &value);
    static ProjectFormat getUnpackedFormat(const std::string &filePath, const std::string &value);

    void init() override;
    void render() override;
    void cleanup() override;
};
