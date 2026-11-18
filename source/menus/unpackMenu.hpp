#pragma once
#include "menuManager.hpp"
#include <string>
#include <thread.hpp>

struct UnpackTask {
    std::string sb3Path;
    std::string unpackedPath;
    SE_Mutex mtx;
    bool done = false;
    bool success = false;
    bool alreadyUnpacked = false;
};

struct BoxThing {
    float x = 0;
    float y = 0;
    float velX = 0;
    float velY = 0;
    int width = 16;
    int height = 16;
};

class SE_EXPORT UnpackMenu : public Menu {
  private:
    std::string projectPath;
    std::string unpackedPath;
    UnpackTask task;
    SE_Thread thread;
    bool threadStarted = false;
    BoxThing thing;

  public:
    UnpackMenu(const std::string &projectPath);
    ~UnpackMenu() override;
    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};