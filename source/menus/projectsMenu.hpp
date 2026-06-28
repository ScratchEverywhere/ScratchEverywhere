#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include "thread.hpp"
#include <memory>
#include <queue>
#include <vector>

class ProjectsMenu : public Menu {
  private:
    std::shared_ptr<Image> missingIcon;
    std::vector<components::ProjectInfo> projects;
    int selectedProject = -1;
    Clay_String noProjectsPath;

    SE_Thread backdropIconThread;
    std::shared_ptr<Image> getProjectIcon(components::ProjectInfo &project, bool unpacked);
    static void backdropLoader(void *userdata);
    std::queue<components::ProjectInfo *> backdropQueue;
    SE_Mutex backdropQueueMutex;
    bool doneLoading = false;
    bool forceExit = false;
    bool threadSpawned = false;
    std::unordered_map<components::ProjectInfo *, std::string> imagePaths;

  public:
    ProjectsMenu(void *userdata);
    ~ProjectsMenu();
    void render() override;
};
