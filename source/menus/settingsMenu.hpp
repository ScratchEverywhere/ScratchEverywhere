#pragma once
#include "audio.hpp"
#include "mainMenu.hpp"
#include "menuObjects.hpp"
#include "window.hpp"
#include <se_export.hpp>

class SE_EXPORT SettingsMenu : public Menu {
  private:
    void updateButtonStates();
    std::string getDectalkString();

  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *Credits = nullptr;
    ButtonObject *EnableUsername = nullptr;
    ButtonObject *ChangeUsername = nullptr;
    ButtonObject *EnableCustomFolderPath = nullptr;
    ButtonObject *ChangeFolderPath = nullptr;
    ButtonObject *EnableMenuMusic = nullptr;
    ButtonObject *RendererButton = nullptr;
    ButtonObject *WindowingButton = nullptr;
    ButtonObject *AudioEngineButton = nullptr;
    ButtonObject *ClearCache = nullptr;
    ButtonObject *Language = nullptr;
    ButtonObject *dectalkButton = nullptr;

    bool UseCostumeUsername = false;
    std::string username;

    bool UseProjectsPath = false;
    std::string projectsPath;

    bool menuMusic = true;

    std::vector<RendererInfo> validRenderers;
    unsigned int rendererIdx = 0;
    std::string rendererOption = "";

    std::vector<WindowingBackendInfo> validWindowingBackends;
    unsigned int windowingIdx = 0;
    std::string windowingOption = "";

    std::vector<AudioBackendInfo> validAudioBackends;
    unsigned int audioIdx = 0;
    std::string audioOption = "";

#ifdef DECTALK_DEFAULT
    bool UseDectalk = true;
#else
    bool UseDectalk = false;
#endif

    SettingsMenu();
    ~SettingsMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};
