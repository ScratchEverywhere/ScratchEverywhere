#include "inspector.hpp"
#ifdef ENABLE_INSPECTOR

#include <blockExecutor.hpp>
#include <iostream>
#include <queue>
#include <runtime.hpp>
#include <sprite.hpp>
#include <sstream>
#include <string>
#include <thread.hpp>
#include <vector>

namespace Inspector {

static SE_Thread cliThread;
static std::queue<std::string> commandQueue;
static SE_Mutex queueMutex;

static Sprite *findSprite(const std::string &name) {
    if (name == "Stage") return Scratch::stageSprite;
    if (!name.empty() && name[0] == '@') {
        try {
            int layer = std::stoi(name.substr(1));
            for (Sprite *s : Scratch::sprites) {
                if (s->layer == layer) return s;
            }
            if (Scratch::stageSprite && Scratch::stageSprite->layer == layer) return Scratch::stageSprite;
        } catch (...) {
        }
    }
    for (Sprite *s : Scratch::sprites) {
        if (s->name == name) return s;
    }
    return nullptr;
}

static std::vector<Sprite *> findSprites(const std::string &name) {
    std::vector<Sprite *> results;
    if (name == "Stage") {
        if (Scratch::stageSprite) results.push_back(Scratch::stageSprite);
        return results;
    }
    if (!name.empty() && name[0] == '@') {
        Sprite *s = findSprite(name);
        if (s) results.push_back(s);
        return results;
    }
    for (Sprite *s : Scratch::sprites) {
        if (s->name == name) results.push_back(s);
    }
    return results;
}

static List *findList(Sprite *target, const std::string &listName) {
    for (auto &[id, l] : target->lists) {
        if (l.name == listName) return &l;
    }
    return nullptr;
}

static std::string parseArg(std::stringstream &ss, bool restOfLine = false) {
    ss >> std::ws;
    std::string result;
    if (ss.peek() == '"') {
        ss.get();
        std::getline(ss, result, '"');
    } else if (restOfLine) {
        std::getline(ss, result);
    } else {
        ss >> result;
    }
    if (!result.empty() && result.back() == '\r') result.pop_back();
    return result;
}

static void loop(void *arg) {
    std::string line;
    std::cout << "Inspector active. Type 'help' for commands.\n";
    while (true) {
        if (!std::getline(std::cin, line)) {
            SE_Thread::sleep(100);
            continue;
        }

        queueMutex.lock();
        commandQueue.push(line);
        queueMutex.unlock();
    }
}

void init() {
    queueMutex.init();
    cliThread.create(loop, nullptr, 0, 1, -2, "Inspector");
    cliThread.detach();
}

void processCommands() {
    queueMutex.lock();
    if (commandQueue.empty()) {
        queueMutex.unlock();
        return;
    }
    std::string line = commandQueue.front();
    commandQueue.pop();
    queueMutex.unlock();

    std::stringstream ss(line);
    std::string cmd;
    ss >> cmd;

    if (cmd.empty()) return;

    if (cmd == "help") {
        std::cout << "Available commands:\n"
                  << "  help                                          - Show this text\n"
                  << "  sprites                                       - List all sprites in layer order\n"
                  << "  status                                        - Show running threads, clones, FPS etc.\n"
                  << "  inspect <sprite or @layer>                    - Show basic sprite info\n"
                  << "  inspectext <sprite or @layer>                 - Show extended sprite info\n"
                  << "  setprop [sprite or @layer:]<prop> <val>       - Set property (x, y, size, rotation, visible)\n"
                  << "  set [sprite or @layer:]<var> <val>            - Set variable value (e.g. Sprite1:score)\n"
                  << "  listadd [sprite or @layer:]<list> <val>       - Add to list\n"
                  << "  listremove [sprite or @layer:]<list> <idx>    - Remove from list\n"
                  << "  listset [sprite or @layer:]<list> <idx> <val> - Set in list\n"
                  << "  listclear [sprite or @layer:]<list>           - Clear list\n"
                  << "  flag                                          - Click Green Flag\n"
                  << "  stop                                          - Stop project\n";
    } else if (cmd == "sprites") {
        std::cout << "Sprites (front tO back):\n";
        for (Sprite *s : Scratch::sprites) {
            std::cout << s->name << " | Vis: " << (s->visible ? "true" : "false") << " | Pos: (" << s->xPosition << ", " << s->yPosition << ") | Layer: " << s->layer << "\n";
        }
    } else if (cmd == "status") {
        std::cout << "Status:\n"
                  << "FPS target: " << Scratch::FPS << "\n"
                  << "Clones: " << Scratch::cloneCount << "/" << Scratch::maxClones << "\n"
                  << "Threads running: " << BlockExecutor::threads.size() << "\n"
                  << "Turbo mode: " << (Scratch::turbo ? "ON" : "OFF") << "\n";
    } else if (cmd == "inspect" || cmd == "inspectext") {
        bool extended = (cmd == "inspectext");
        std::string spriteName = parseArg(ss, true);
        Sprite *target = findSprite(spriteName);

        if (!target) {
            std::cout << "Sprite '" << spriteName << "' not found.\n";
        } else {
            std::cout << "Sprite: " << target->name << (target->isStage ? " (Stage)" : "") << "\n"
                      << "Position: (" << target->xPosition << ", " << target->yPosition << ")\n"
                      << "Rotation: " << target->rotation << "\n"
                      << "Size: " << target->size << "%\n"
                      << "Visible: " << (target->visible ? "true" : "false") << "\n"
                      << "Layer: " << target->layer << "\n"
                      << "Is Clone: " << (target->isClone ? "true" : "false") << "\n"
                      << "Costumes: " << target->costumes.size() << " (Current: " << target->currentCostume << ")\n"
                      << "Sounds: " << target->sounds.size() << "\n";

            if (extended) {
                std::cout << "--- Extended Info ---\n"
                          << "Draggable: " << (target->draggable ? "true" : "false") << "\n"
                          << "To Delete: " << (target->toDelete ? "true" : "false") << "\n"
                          << "Effects: [Ghost: " << target->ghostEffect << ", Brightness: " << target->brightnessEffect << ", Color: " << target->colorEffect << "]\n"
                          << "Audio: [Volume: " << target->volume << ", Pitch: " << target->pitch << ", Pan: " << target->pan << "]\n"
                          << "Rotation Style: " << (target->rotationStyle == Sprite::ALL_AROUND ? "all around" : (target->rotationStyle == Sprite::LEFT_RIGHT ? "left-right" : "none")) << "\n"
                          << "Dimensions: " << target->spriteWidth << "x" << target->spriteHeight << "\n"
                          << "Pen: [Down: " << (target->penData.down ? "true" : "false") << ", Size: " << target->penData.size << ", Hue: " << target->penData.color.hue << ", Sat: " << target->penData.color.saturation << ", Bri: " << target->penData.color.brightness << ", Shade: " << target->penData.shade << "]\n"
                          << "TTS: [Gender: " << target->textToSpeechData.gender << ", Lang: " << target->textToSpeechData.language << "]\n";
            }

            std::cout << "- Variables:\n";
            for (auto &[id, v] : target->variables) {
                std::cout << "  " << v.name << " = " << v.value.asString() << "\n";
            }
            std::cout << "- Lists:\n";
            for (auto &[id, l] : target->lists) {
                std::cout << "  " << l.name << " (length " << l.items.size() << ")\n";
                if (!extended && l.items.size() > 10) {
                    for (size_t i = 0; i < 10; i++) {
                        std::cout << "    [" << i + 1 << "] " << l.items[i].asString() << "\n";
                    }
                    std::cout << "    ... and " << (l.items.size() - 10) << " more. Use inspectext to see all.\n";
                } else {
                    for (size_t i = 0; i < l.items.size(); i++) {
                        std::cout << "    [" << i + 1 << "] " << l.items[i].asString() << "\n";
                    }
                }
            }
        }
    } else if (cmd == "vars") {
        std::cout << "Global Variables:\n";
        if (Scratch::stageSprite) {
            for (auto &[id, v] : Scratch::stageSprite->variables) {
                std::cout << "  " << v.name << " = " << v.value.asString() << "\n";
            }
        }
    } else if (cmd == "set") {
        std::string targetStr = parseArg(ss, false);
        std::string valStr = parseArg(ss, true);

        size_t colon = targetStr.find(':');
        if (colon == std::string::npos) {
            // Global (Stage)
            if (!Scratch::stageSprite) {
                std::cout << "Stage not found.\n";
                return;
            }
            bool found = false;
            for (auto &[id, v] : Scratch::stageSprite->variables) {
                if (v.name == targetStr) {
                    v.value = Value(valStr);
                    std::cout << "Set global " << targetStr << " = " << valStr << "\n";
                    found = true;
                    break;
                }
            }
            if (!found) std::cout << "Global variable '" << targetStr << "' not found.\n";
        } else {
            std::string spriteName = targetStr.substr(0, colon);
            std::string varName = targetStr.substr(colon + 1);
            auto targets = findSprites(spriteName);
            if (targets.empty()) {
                std::cout << "No sprites matching '" << spriteName << "' found.\n";
            } else {
                int count = 0;
                for (Sprite *s : targets) {
                    for (auto &[id, v] : s->variables) {
                        if (v.name == varName) {
                            v.value = Value(valStr);
                            count++;
                            break;
                        }
                    }
                }
                if (count > 0) {
                    std::cout << "Set " << varName << " = " << valStr << " for " << count << " instance(s) of " << spriteName << ".\n";
                } else {
                    std::cout << "Variable '" << varName << "' not found in sprite(s) '" << spriteName << "'.\n";
                }
            }
        }
    } else if (cmd == "setprop") {
        std::string targetStr = parseArg(ss, false);
        std::string valStr = parseArg(ss, true);

        std::string spriteName = "Stage";
        std::string prop = targetStr;

        size_t colon = targetStr.find(':');
        if (colon != std::string::npos) {
            spriteName = targetStr.substr(0, colon);
            prop = targetStr.substr(colon + 1);
        }

        auto targets = findSprites(spriteName);
        if (targets.empty()) {
            std::cout << "No sprites matching '" << spriteName << "' found.\n";
            return;
        }

        int count = 0;
        for (Sprite *target : targets) {
            try {
                if (prop == "x") {
                    target->xPosition = std::stof(valStr);
                } else if (prop == "y") {
                    target->yPosition = std::stof(valStr);
                } else if (prop == "size") {
                    target->size = std::stof(valStr);
                } else if (prop == "rotation") {
                    target->rotation = std::stof(valStr);
                } else if (prop == "visible") {
                    target->visible = (valStr == "true" || valStr == "1");
                } else {
                    std::cout << "Unknown property '" << prop << "'. Use: x, y, size, rotation, visible.\n";
                    return;
                }
                count++;
            } catch (...) {
                std::cout << "Failed to parse value '" << valStr << "' for property '" << prop << "'.\n";
                return;
            }
        }
        std::cout << "Set property " << prop << " to " << valStr << " for " << count << " instance(s).\n";
    } else if (cmd == "listadd" || cmd == "listremove" || cmd == "listset" || cmd == "listclear") {
        std::string targetStr = parseArg(ss, false);
        std::string arg1 = parseArg(ss, false); // val for add/clear(rest), idx for remove/set
        std::string arg2 = parseArg(ss, true);  // val for set, empty for others

        std::string spriteName = "Stage";
        std::string listName = targetStr;

        size_t colon = targetStr.find(':');
        if (colon != std::string::npos) {
            spriteName = targetStr.substr(0, colon);
            listName = targetStr.substr(colon + 1);
        }

        auto targets = findSprites(spriteName);
        if (targets.empty()) {
            std::cout << "No sprites matching '" << spriteName << "' found.\n";
            return;
        }

        int count = 0;
        for (Sprite *target : targets) {
            List *list = findList(target, listName);
            if (!list) continue;

            if (cmd == "listadd") {
                list->items.push_back(Value(arg1));
            } else if (cmd == "listremove") {
                try {
                    int idx = std::stoi(arg1) - 1;
                    if (idx >= 0 && idx < (int)list->items.size()) {
                        list->items.erase(list->items.begin() + idx);
                    }
                } catch (...) {
                }
            } else if (cmd == "listset") {
                try {
                    int idx = std::stoi(arg1) - 1;
                    if (idx >= 0 && idx < (int)list->items.size()) {
                        list->items[idx] = Value(arg2);
                    }
                } catch (...) {
                }
            } else if (cmd == "listclear") {
                list->items.clear();
            }
            count++;
        }
        if (count > 0) {
            std::cout << "Executed " << cmd << " on " << count << " instance(s) of list '" << listName << "'.\n";
        } else {
            std::cout << "List '" << listName << "' not found in specified sprite(s).\n";
        }
    } else if (cmd == "flag") {
        Scratch::greenFlagClicked();
        std::cout << "Green flag clicked.\n";
    } else if (cmd == "stop") {
        Scratch::stopClicked();
        std::cout << "Project stopped.\n";
    } else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
    }
}

} // namespace Inspector

#else

namespace Inspector {
void init() {}
void processCommands() {}
} // namespace Inspector

#endif
