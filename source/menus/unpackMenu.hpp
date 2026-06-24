#pragma once
#include "clay.h"
#include "image.hpp"
#include "menu.hpp"
#include "os.hpp"
#include "settingsMenu.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <thread.hpp>

struct UnpackParams {
    std::string projectName;
    bool deletingProject;
};

class UnpackMenu : public Menu {
  private:
    SE_Thread thread;

    std::string projectName;
    bool deletingProject;
    Clay_String title;
    std::string text;

    static void addToJsonArray(const std::string &filePath, const std::string &value);

    static void unpack(void *data);

    std::vector<std::string> getJsonArray(const std::string &filePath);

    static void removeFromJsonArray(const std::string &filePath, const std::string &value);

  public:
    UnpackMenu(void *userdata = nullptr, const std::string &title = "Unpack Menu");
    ~UnpackMenu();
    void render() override;
};
