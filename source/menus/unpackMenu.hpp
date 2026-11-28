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

class SE_EXPORT UnpackMenu : public Menu {
  private:
    std::string projectPath;
    std::string unpackedPath;
    UnpackTask task;
    SE_Thread thread;
    bool threadStarted = false;

  public:
    UnpackMenu(const std::string &projectPath);
    ~UnpackMenu() override;
    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};