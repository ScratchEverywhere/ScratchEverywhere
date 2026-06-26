#include "controlsMenu.hpp"
#include "menuManager.hpp"
#include "runtime.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <log.hpp>
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

    for (auto &block : Scratch::blocks) {
        std::string buttonCheck;
        if (block->opcode == "sensing_keypressed") {
            if (block->inputs["KEY_OPTION"].inputType == ParsedInput::VALUE) {
                buttonCheck = block->inputs["KEY_OPTION"].value.asString();
            }
        } else if (block->opcode == "event_whenkeypressed") {
            buttonCheck = block->fields["KEY_OPTION"].value;
        } else if (block->opcode == "makeymakey_whenMakeyKeyPressed") {
            if (block->inputs["KEY"].inputType == ParsedInput::VALUE) {
                buttonCheck = block->inputs["KEY"].value.asString();
            }
        } else if (block->opcode == "makeymakey_whenCodePressed") {
            if (block->inputs["SEQUENCE"].inputType != ParsedInput::VALUE) continue;

            std::string input = block->inputs["SEQUENCE"].value.asString();
            size_t start = 0;
            size_t end;
            while ((end = input.find(' ', start)) != std::string::npos) {
                buttonCheck = input.substr(start, end - start);
                if (buttonCheck != "") {
                    break;
                }
                start = end + 1;
            }
            buttonCheck = input.substr(start);
        } else continue;
        if (buttonCheck != "" && controls.find(buttonCheck) == controls.end()) {
            Log::log("Found new control: " + buttonCheck);
            for (const auto &[key, val] : Input::inputControls) {
                if (val == buttonCheck)
                    controls[buttonCheck] = key;
            }
        }
    }

    Scratch::cleanupScratchProject();

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
        menuManager->queueChangeMenu(MenuID::ProjectsMenu);
        return;
    }
}
