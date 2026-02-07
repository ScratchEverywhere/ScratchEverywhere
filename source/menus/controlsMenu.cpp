#include "controlsMenu.hpp"
#include "menuManager.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <render.hpp>
#include <settings.hpp>
#include <unzip.hpp>

ControlsMenu::ControlsMenu(void *userdata) {
    projectName = static_cast<char *>(userdata);
    settings = SettingsManager::getProjectSettings(projectName);
    const std::string projectPath = OS::getScratchFolderLocation() + projectName + ".sb3";

    mz_zip_archive zip{};
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, projectPath.c_str(), 0)) {
        Log::logError("Could not find project: " + projectPath);
        return;
    }
    int fileIndex = mz_zip_reader_locate_file(&zip, "project.json", nullptr, 0);

    if (fileIndex < 0) {
        Log::logError("Could not find project.json: " + projectPath);
        return;
    }

    size_t extractedSize = 0;
    void *data = mz_zip_reader_extract_to_heap(&zip, fileIndex, &extractedSize, 0);

    if (!data) {
        Log::logError("Could not extract project.json: " + projectPath);
        return;
    }

    const char *begin = static_cast<const char *>(data);
    const char *end = begin + extractedSize;
    nlohmann::json j = nlohmann::json::parse(begin, end);
    nlohmann::json controlSettings = SettingsManager::getProjectSettings(projectName);
    Parser::loadSprites(j);
    Input::applyControls(projectPath + ".json");

    for (auto &sprite : Scratch::sprites) {
        for (auto &[id, block] : sprite->blocks) {
            std::string buttonCheck;
            if (block.opcode == "sensing_keypressed") {
                buttonCheck = Input::convertToKey(Scratch::getInputValue(block, "KEY_OPTION", sprite));
            } else if (block.opcode == "event_whenkeypressed") {
                buttonCheck = Input::convertToKey(Value(Scratch::getFieldValue(block, "KEY_OPTION")));
            } else if (block.opcode == "makeymakey_whenMakeyKeyPressed") {
                buttonCheck = Input::convertToKey(Scratch::getInputValue(block, "KEY", sprite), true);
            } else if (block.opcode == "makeymakey_whenCodePressed") {
                std::string input = Scratch::getInputValue(block, "SEQUENCE", sprite).asString();
                size_t start = 0;
                size_t end;
                while ((end = input.find(' ', start)) != std::string::npos) {
                    buttonCheck = input.substr(start, end - start);
                    if (buttonCheck != "" && controls.find(buttonCheck) == controls.end()) {
                        // Log::log("Found new control: " + buttonCheck);
                        for (const auto &[key, val] : Input::inputControls) {
                            if (val == buttonCheck)
                                controls[buttonCheck] = key;
                        }
                    }
                    start = end + 1;
                }
                buttonCheck = input.substr(start);
                if (buttonCheck != "" && controls.find(buttonCheck) == controls.end()) {
                    // Log::log("Found new control: " + buttonCheck);
                    for (const auto &[key, val] : Input::inputControls) {
                        if (val == buttonCheck)
                            controls[buttonCheck] = key;
                    }
                }
                continue;
            } else continue;
            if (buttonCheck != "" && controls.find(buttonCheck) == controls.end()) {
                // Log::log("Found new control: " + buttonCheck);
                for (const auto &[key, val] : Input::inputControls) {
                    if (val == buttonCheck)
                        controls[buttonCheck] = key;
                }
            }
        }
    }

    Scratch::cleanupSprites();
    Scratch::cleanupSprites();
    Render::monitorTexts.clear();
    Render::listMonitors.clear();
    Render::visibleVariables.clear();

    // Reset Runtime (should maybe add a Scratch::cleanupRuntime() function,)
    Scratch::broadcastQueue.clear();
    Scratch::cloneQueue.clear();
    Scratch::stageSprite = nullptr;
    Scratch::answer.clear();
    Scratch::customUsername.clear();
    Scratch::projectWidth = 480;
    Scratch::projectHeight = 360;
    Scratch::cloneCount = 0;
    Scratch::maxClones = 300;
    Scratch::FPS = 30;
    Scratch::counter = 0;
    Scratch::turbo = false;
    Scratch::hqpen = false;
    Scratch::fencing = true;
    Scratch::miscellaneousLimits = true;
    Scratch::shouldStop = false;
    Scratch::forceRedraw = false;
    Scratch::nextProject = false;
    Scratch::useCustomUsername = false;
    Scratch::projectType = UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;
    Input::applyControls("");

    for (auto &[controlName, controlValue] : controls) {
        controlStrings[controlName] = controlName + " --> " + controlValue;
        Timer t;
        hoverData.insert({controlName, {settings, controlName, t}});
        clayIds[controlName] = {false, static_cast<int32_t>((controlName).length()), controlName.c_str()};
    }

    init("Custom Controls");
}

ControlsMenu::~ControlsMenu() {
    if (controls.empty()) return;

    nlohmann::json settings = SettingsManager::getProjectSettings(projectName);
    for (const auto &c : controls) {
        settings["controls"][c.first] = c.second;
    }
    SettingsManager::saveProjectSettings(settings, projectName);
    return;
}

void ControlsMenu::renderSettings() {
    if (controls.empty()) {
        Log::logWarning("No controls found in project");
        menuManager->back(const_cast<void *>(static_cast<const void *>(projectName.c_str())));
        return;
    }

    for (auto &[controlName, controlValue] : controls) {

        renderOrder.push_back(controlName);

        const uint16_t hPadding = 10 * menuManager->scale;
        const uint16_t vPadding = 5 * menuManager->scale;
        Clay_String str = {false, static_cast<int>(controlStrings[controlName].size()), controlStrings[controlName].c_str()};
        // clang-format off
        CLAY(CLAY_SID(clayIds[controlName]),(Clay_ElementDeclaration){
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                .padding = {hPadding, hPadding, vPadding, vPadding}
            },
            .backgroundColor = {90, 60, 90, 255},
            .cornerRadius = {5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale}
        }) {
            Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
                const auto hoverData = reinterpret_cast<Settings_HoverData*>(userdata);
            	if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
            		hoverData->justPressed = true;
            	} else hoverData->justPressed = false;
            }, &hoverData.at(controlName));

            CLAY_TEXT((str), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
        }
        // clang-format on

        if (hoverData.at(controlName).justPressed && hoverData.at(controlName).controlSetState == 0) hoverData.at(controlName).controlSetState = 1;
    }

    for (auto &[controlName, controlValue] : controls) {

        switch (hoverData.at(controlName).controlSetState) {
        case 1: {
            Input::getInput();
            bool end = true;
            for (auto &[button, time] : Input::keyHeldDuration) {
                if (button == "any") continue;
                if (time > 0) {
                    end = false;
                    break;
                }
            }
            if (end) {
                hoverData.at(controlName).controlSetState = 2;
                menuManager->canChangeMenus = false;
                controlStrings[controlName] = "Waiting for input..";
            }
            break;
        }
        case 2: {
            Input::getInput();
            for (auto &[button, time] : Input::keyHeldDuration) {
                if (button == "any") continue;
                if (time > 0) {
                    std::string newButton = "";
                    for (const auto &[key, val] : Input::inputControls) {
                        if (val == button) newButton = key;
                    }
                    controls[controlName] = newButton;
                    controlStrings[controlName] = controlName + " --> " + newButton;
                    hoverData.at(controlName).controlSetState = 0;
                    hoverData.at(controlName).justPressed = false;
                    menuManager->canChangeMenus = true;
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    if (Input::isButtonJustPressed("B") && menuManager->canChangeMenus) {
        menuManager->changeMenu(MenuID::ProjectsMenu);
        return;
    }
}
