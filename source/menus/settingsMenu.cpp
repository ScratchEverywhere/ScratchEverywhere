#include "settingsMenu.hpp"
#include "discord.hpp"
#include "menuManager.hpp"
#include "os.hpp"
#include "renderer.hpp"
#include "ui.hpp"
#include <filesystem.hpp>
#include <log.hpp>
#include <settings.hpp>
#include <translation.hpp>

bool SettingsMenu::isToggled(const std::string &jsonKey) {
    return (json.contains(jsonKey) && json[jsonKey].is_boolean() && json[jsonKey].get<bool>());
}

void SettingsMenu::toggle(UI::Builder &b, const std::string &jsonKey, const std::string &name, bool defaultValue, std::function<void(bool)> onChange) {

    bool toggled = defaultValue;
    if (json.contains(jsonKey) && json[jsonKey].is_boolean()) {
        toggled = json[jsonKey].get<bool>();
    }

    const UI::Color color = toggled ? UI::Color{255, 151, 46, 255} : UI::Color{67, 57, 34, 255};

    b.element({.flags = UI::Flag::Focusable,
               .w = UI::Size::fit(),
               .h = UI::Size::fit(),
               .childDirection = UI::ChildDir::LeftRight,
               .childGap = 5,
               .padding = UI::Padding::axis(5, 0),
               .style = UI::StyleId::TransparentButton,
               .onClick = [jsonKey, toggled, onChange, &b, this] {
                   bool newState = !toggled;
                   this->json[jsonKey] = newState;
                   if (onChange) {
                       onChange(newState);
                   }
                   b.dirty = true;
               }},
              [&] {
                  b.element({
                      .w = UI::Size::fixed(32),
                      .h = UI::Size::fixed(32),
                      .color = color,
                  });
                  b.text({.text = TranslationManager::getTranslation(name),
                          .color = {255, 255, 255, 255},
                          .align = UI::AlignMode::Left});
              });
}

void SettingsMenu::button(UI::Builder &b, const std::string &name, std::function<void()> onClick) {

    b.element({.flags = UI::Flag::Focusable,
               .w = UI::Size::fit(),
               .h = UI::Size::fit(),
               .childDirection = UI::ChildDir::LeftRight,
               .childGap = 5,
               .padding = UI::Padding::all(8),
               .style = UI::StyleId::Button,
               .onClick = [&b, onClick] {
                   if (onClick) {
                       onClick();
                   }
                   b.dirty = true;
               }},
              [&] {
                  b.text({.text = TranslationManager::getTranslation(name),
                          .color = {255, 255, 255, 255},
                          .align = UI::AlignMode::Center});
              });
}

// not really a dropdown beacuse LAAAAAZZZYYY
void SettingsMenu::dropdown(UI::Builder &b, const std::string &jsonKey, const std::string &name, const std::vector<std::string> &options, const std::string &fallback) {
    if (options.size() < 2) return;

    std::string current = fallback;
    if (json.contains(jsonKey) && json[jsonKey].is_string())
        current = json[jsonKey].get<std::string>();

    size_t index = 0;
    const auto it = std::find(options.begin(), options.end(), current);
    if (it != options.end()) index = it - options.begin();
    else current = options[0];

    const std::string next = options[(index + 1) % options.size()];

    b.element({.flags = UI::Flag::Focusable,

               .childDirection = UI::ChildDir::LeftRight,
               .childGap = 10,
               .padding = UI::Padding::axis(5, 0),
               .style = UI::StyleId::TransparentButton,
               .onClick = [this, &b, jsonKey, next] {
                   json[jsonKey] = next;
                   b.dirty = true;
               }},
              [&] {
                  b.text({.text = TranslationManager::getTranslation(name),
                          .color = {255, 255, 255, 255},
                          .align = UI::AlignMode::Left});
                  b.element({
                                .w = UI::Size::fit(),
                                .h = UI::Size::fit(),
                                .padding = UI::Padding::all(5),
                                .style = UI::StyleId::Button,
                            },
                            [&] {
                                b.text({
                                    .text = current,
                                    .fontId = 1,
                                    .color = {255, 255, 255, 255},
                                    .align = UI::AlignMode::Center,
                                });
                            });
                  b.text({
                      .text = std::to_string(index + 1) + " / " + std::to_string(options.size()),
                      .fontId = 1,
                      .scale = 0.95,
                      .color = {255, 255, 255, 255},

                  });
              });
}

void SettingsMenu::section(UI::Builder &b, const std::string &name) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fit(),
                  .childDirection = UI::ChildDir::UpDown,
                  .childGap = 5,
              },
              [&] {
                  b.text({.text = name, .scale = 1.15, .color = {255, 255, 255, 255}});
                  b.element({
                      .w = UI::Size::grow(),
                      .h = UI::Size::fixed(2),
                      .color = {255, 255, 255, 255},
                  });
              });
}

GlobalSettingsMenu::GlobalSettingsMenu() {
    json = SettingsManager::getConfigSettings();
    rendererList = Render::getAvailableRenderers();
    windowingList = WindowSE::getAvailableWindowingBackends();
    audioEngineList = SoundPlayer::getAvailableAudioBackends();
}

GlobalSettingsMenu::~GlobalSettingsMenu() {
    SettingsManager::saveConfigSettings(json);
}

void GlobalSettingsMenu::changeProjectPath() {
    std::string projectsPath = json.contains("ProjectsPath") && json["ProjectsPath"].is_string() ? json["ProjectsPath"].get<std::string>() : OS::getScratchFolderLocation();

#if defined(USE_LIBDLGMOD)
    // FIXME: Translate this into every localization supported by SE!
    const char *folder_picker_dialog_titlebar_caption = "Select a custom path to load *.sb3 Scratch project files...";

#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS) && !defined(LIBRETRO)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))

    std::string newPathGui = get_directory_alt(folder_picker_dialog_titlebar_caption, "");

#if defined(_WIN32) || defined(_WIN64)
    std::replace(newPathGui.begin(), newPathGui.end(), '\\', '/'); // Normalize path separators
#endif

    const std::string newPath = ((newPathGui.empty()) ? ((!hasdeps()) ? Input::openSoftwareKeyboard(projectsPath.c_str()) : projectsPath) : newPathGui);

#endif

#else

    const std::string newPath = Input::openSoftwareKeyboard(projectsPath.c_str());

#endif

    if (newPath.length() > 0) {
        projectsPath = newPath;
    }

    json["ProjectsPath"] = projectsPath;

    // uhhh what do these do? are these even needed in new UI? err
    OS::customProjectsPath = nullptr;
    OS::loadedSettings = false;
}

void GlobalSettingsMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = {5, 5, 20, 0},
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = "Global Settings", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                section(b, "Menu");

                                button(b, "ui.settings.language", [&] {
                                    MenuManager::changeMenu(MenuId::LanguageMenu);
                                    return;
                                });

                                toggle(b, "MenuMusic", "ui.settings.music", true, [](bool enabled) {
                                    if (enabled) MenuManager::initMenuMusic();
                                    else MenuManager::deinitMenuMusic();
                                });

                                if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation()) {
                                    toggle(b, "UseProjectsPath", "ui.settings.path", false);
                                    if (isToggled("UseProjectsPath")) {
                                        button(b, "ui.settings.changePath", [&] {
                                            changeProjectPath();
                                        });
                                    }
                                }

                                button(b, "ui.settings.cache", [&] {
                                    const auto rderr = FileSystem::removeDirectory(OS::getScratchFolderLocation() + "cache/");
                                    const auto cderr = FileSystem::createDirectory(OS::getScratchFolderLocation() + "cache/");
                                    if (!rderr.has_value() || !cderr.has_value()) Log::logCritical("Failed to clear cache.", false);
                                });

                                section(b, "Project");

                                toggle(b, "EnableUsername", "ui.settings.username", false);
                                if (isToggled("EnableUsername")) {
                                    button(b, "ui.settings.name", [&] {
                                        std::string username = json.contains("Username") && json["Username"].is_string() ? json["Username"].get<std::string>() : "";
                                        std::string newUsername = Input::openSoftwareKeyboard(username.c_str());
                                        // You could also use regex here, Idk what would be more sensible
                                        // std::regex_match(s, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))
                                        if (newUsername.length() <= 9) {
                                            bool hasNonSpace = false;
                                            for (char c : newUsername) {
                                                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                                                    hasNonSpace = true;
                                                } else if (!std::isspace(static_cast<unsigned char>(c))) {
                                                    break;
                                                }
                                            }
                                            if (hasNonSpace) username = newUsername;
                                        }
                                        json["Username"] = username;
                                    });
                                }
#ifdef ENABLE_DECTALK
                                toggle(b, "UseDectalk", "ui.settings.dectalk", false);
#endif

                                if (rendererList.size() >= 2 || windowingList.size() >= 2 || audioEngineList.size() >= 2) {
                                    section(b, "Advanced");

                                    dropdown(b, "Renderer", "ui.settings.renderer", rendererList, RendererNames[static_cast<uint8_t>(Render::currentRenderer)]);
                                    dropdown(b, "Windowing", "ui.settings.windowing", windowingList, WindowingBackendNames[static_cast<uint8_t>(WindowSE::currentWindowingBackend)]);
                                    dropdown(b, "AudioEngine", "ui.settings.audioEngine", audioEngineList, AudioBackendNames[static_cast<uint8_t>(SoundPlayer::currentSoundBackend)]);
                                }
#ifdef ENABLE_DISCORD_RPC
                                section(b, "Other");

                                toggle(b, "DiscordPresence", "ui.settings.discord", true, [](bool newState) {
                                    Discord::setEnabled(newState);
                                });
#endif
                            });
              });
}