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
                  << "  help                           - Show this text\n"
                  << "  sprites                        - List all sprites in layer order\n"
                  << "  status                         - Show running threads, clones, FPS etc.\n"
                  << "  inspect <sprite_name>          - Show sprite info, vars and lists\n"
                  << "  vars                           - Show global variables\n"
                  << "  set <var> <val>                - Set global variable value\n"
                  << "  setlocal <sprite> <var> <val>  - Set local variable value\n"
                  << "  flag                           - Click Green Flag\n"
                  << "  stop                           - Stop project\n";
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
    } else if (cmd == "inspect") {
        std::string spriteName = parseArg(ss, true);

        Sprite *target = nullptr;
        for (Sprite *s : Scratch::sprites) {
            if (s->name == spriteName) {
                target = s;
                break;
            }
        }
        if (!target && spriteName == "Stage") target = Scratch::stageSprite;

        if (!target) {
            std::cout << "Sprite '" << spriteName << "' not found.\n";
        } else {
            std::cout << "Sprite: " << target->name << "\n"
                      << "Position: (" << target->xPosition << ", " << target->yPosition << ")\n"
                      << "Rotation: " << target->rotation << "\n"
                      << "Size: " << target->size << "%\n"
                      << "Visible: " << (target->visible ? "true" : "false") << "\n"
                      << "Layer: " << target->layer << "\n"
                      << "Is Clone: " << (target->isClone ? "true" : "false") << "\n"
                      << "- Variables:\n";
            for (auto &[id, v] : target->variables) {
                std::cout << "  " << v.name << " = " << v.value.asString() << "\n";
            }
            std::cout << "- Lists:\n";
            for (auto &[id, l] : target->lists) {
                std::cout << "  " << l.name << " (length " << l.items.size() << ")\n";
                for (size_t i = 0; i < l.items.size(); i++) {
                    std::cout << "    [" << i + 1 << "] " << l.items[i].asString() << "\n";
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
        std::string varName = parseArg(ss, false);
        std::string valStr = parseArg(ss, true);

        bool found = false;
        if (Scratch::stageSprite) {
            for (auto &[id, v] : Scratch::stageSprite->variables) {
                if (v.name == varName) {
                    v.value = Value(valStr);
                    std::cout << "Set " << varName << " = " << valStr << "\n";
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            std::cout << "Global variable '" << varName << "' not found.\n";
        }
    } else if (cmd == "setlocal") {
        std::string spriteName = parseArg(ss, false);
        std::string varName = parseArg(ss, false);
        std::string valStr = parseArg(ss, true);

        Sprite *target = nullptr;
        for (Sprite *s : Scratch::sprites) {
            if (s->name == spriteName) {
                target = s;
                break;
            }
        }
        if (!target) {
            std::cout << "Sprite '" << spriteName << "' not found.\n";
            return;
        }

        bool found = false;
        for (auto &[id, v] : target->variables) {
            if (v.name == varName) {
                v.value = Value(valStr);
                std::cout << "Set " << spriteName << "'s " << varName << " = " << valStr << "\n";
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Local variable '" << varName << "' not found in sprite '" << spriteName << "'.\n";
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
