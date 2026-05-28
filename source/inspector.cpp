#include "inspector.hpp"
#ifdef ENABLE_INSPECTOR

#include <blockExecutor.hpp>
#include <iostream>
#include <queue>
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <sprite.hpp>
#include <sstream>
#include <string>
#include <thread.hpp>
#include <vector>

namespace Inspector {

struct WatchEntry {
    std::string targetStr;
    Value lastValue;
};
static std::vector<WatchEntry> watchedVars;

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
    if (!commandQueue.empty()) {
        std::string line = commandQueue.front();
        commandQueue.pop();
        queueMutex.unlock();

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "help") {
            std::string subCmd = parseArg(ss, false);
            if (subCmd.empty()) {
                std::cout << "\nInspector Commands (use 'help <cmd>' for more)\n"
                          << "[General]\n"
                          << "  help, status,    - Help, project status\n"
                          << "  sprites, clones,    - List all sprites and clones\n"
                          << "[Inspection]\n"
                          << "  inspect, inspectext      - Basic vs extended sprite data\n"
                          << "  dist, touching           - Spatial & collision checks\n"
                          << "[Variables & Lists]\n"
                          << "  set, setprop, broadcast  - Update data or trigger events\n"
                          << "  listadd, listremove, listset, listclear\n"
                          << "[Monitoring]\n"
                          << "  watch, unwatch, clearwatch - Real-time variable tracking\n"
                          << "[Control]\n"
                          << "  flag, stop               - Start or stop project execution\n"
                          << "Syntax: Use 'SpriteName:Var' for locals, '@Layer' for specefic sprite at a certain layer (including stage and clones).\n\n";
            } else if (subCmd == "inspect" || subCmd == "inspectext") {
                std::cout << "Usage: " << subCmd << " <name or @layer>\n"
                          << "Prints transform, variables, and lists. 'inspectext' adds effects, audio, pen, and full lists.\n";
            } else if (subCmd == "set" || subCmd == "setprop") {
                std::cout << "Usage: " << subCmd << " [sprite:]<name> <value>\n"
                          << "Updates vars or properties (x, y, size, rotation, visible).\n"
                          << "If using a name instead of @layer, all clones of that name are updated.\n";
            } else if (subCmd == "watch") {
                std::cout << "Usage: watch [sprite:]<var>\n"
                          << "Prints a log whenever the variable's value changes during a tick.\n";
            } else if (subCmd == "dist" || subCmd == "touching") {
                std::cout << "Usage: " << subCmd << " <s1> <s2>\n"
                          << "Check distance or if two sprites are colliding. name or @layer supported.\n";
            } else if (subCmd == "listset" || subCmd == "listadd" || subCmd == "listremove" || subCmd == "listclear") {
                std::cout << "List commands use 1-based indexing.\n"
                          << "Example: listset Sprite1:myList 1 newItem\n";
            } else {
                std::cout << "No extra info for '" << subCmd << "'.\n";
            }
        } else if (cmd == "sprites" || cmd == "clones") {
            bool onlyClones = (cmd == "clones");
            std::cout << "Sprites (front to back):\n";
            SpeechManager *sm = Render::getSpeechManager();
            for (Sprite *s : Scratch::sprites) {
                if (onlyClones && !s->isClone) continue;
                std::cout << s->name << " | Vis: " << (s->visible ? "true" : "false") << " | Pos: (" << s->xPosition << ", " << s->yPosition << ") | Layer: " << s->layer;
                if (sm) {
                    std::string text = sm->getSpeechText(s);
                    if (!text.empty()) {
                        std::cout << " | " << sm->getSpeechStyle(s) << ": \"" << text << "\"";
                    }
                }
                std::cout << "\n";
            }
        } else if (cmd == "status") {
            std::cout << "Status:\n"
                      << "FPS target: " << Scratch::FPS << "\n"
                      << "Clones: " << Scratch::cloneCount << "/" << Scratch::maxClones << "\n"
                      << "Threads running: " << BlockExecutor::threads.size() << "\n"
                      << "Pools: States=" << Pools::states.size() << ", Threads=" << Pools::threads.size() << "\n"
                      << "Timer: " << (BlockExecutor::timer.getTimeMs() / 1000.0) << "s\n"
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
        } else if (cmd == "set") {
            std::string targetStr = parseArg(ss, false);
            std::string valStr = parseArg(ss, true);
            size_t colon = targetStr.find(':');
            if (colon == std::string::npos) {
                if (Scratch::stageSprite) {
                    for (auto &[id, v] : Scratch::stageSprite->variables) {
                        if (v.name == targetStr) {
                            v.value = Value(valStr);
                            std::cout << "Set global " << targetStr << " = " << valStr << "\n";
                            break;
                        }
                    }
                }
            } else {
                std::string spriteName = targetStr.substr(0, colon);
                std::string varName = targetStr.substr(colon + 1);
                for (Sprite *s : findSprites(spriteName)) {
                    for (auto &[id, v] : s->variables) {
                        if (v.name == varName) v.value = Value(valStr);
                    }
                }
            }
        } else if (cmd == "setprop") {
            std::string targetStr = parseArg(ss, false);
            std::string valStr = parseArg(ss, true);
            std::string spriteName = "Stage", prop = targetStr;
            size_t colon = targetStr.find(':');
            if (colon != std::string::npos) {
                spriteName = targetStr.substr(0, colon);
                prop = targetStr.substr(colon + 1);
            }
            for (Sprite *target : findSprites(spriteName)) {
                if (prop == "x") target->xPosition = std::stof(valStr);
                else if (prop == "y") target->yPosition = std::stof(valStr);
                else if (prop == "size") target->size = std::stof(valStr);
                else if (prop == "rotation") target->rotation = std::stof(valStr);
                else if (prop == "visible") target->visible = (valStr == "true" || valStr == "1");
            }
        } else if (cmd == "broadcast") {
            std::string name = parseArg(ss, true);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            for (auto &spr : Scratch::sprites) {
                if (spr->hats["event_whenbroadcastreceived"].empty()) continue;
                for (Block *hat : spr->hats["event_whenbroadcastreceived"]) {
                    std::string broadcastOption = Scratch::getFieldValue(*hat, "BROADCAST_OPTION");
                    std::transform(broadcastOption.begin(), broadcastOption.end(), broadcastOption.begin(), ::tolower);
                    if (broadcastOption == name) {
                        BlockExecutor::startThread(spr, hat);
                    }
                }
            }
            std::cout << "Broadcasted '" << name << "'.\n";
        } else if (cmd == "dist") {
            std::string s1Name = parseArg(ss, false), s2Name = parseArg(ss, false);
            Sprite *s1 = findSprite(s1Name), *s2 = findSprite(s2Name);
            if (s1 && s2) std::cout << "Distance: " << sqrt(pow(s1->xPosition - s2->xPosition, 2) + pow(s1->yPosition - s2->yPosition, 2)) << "\n";
        } else if (cmd == "touching") {
            std::string s1Name = parseArg(ss, false), s2Name = parseArg(ss, false);
            Sprite *s1 = findSprite(s1Name), *s2 = findSprite(s2Name);
            if (s1 && s2) std::cout << (Scratch::isColliding("sprite", s1, s2) ? "YES\n" : "NO\n");
        } else if (cmd == "watch") {
            std::string target = parseArg(ss, true);
            watchedVars.push_back({target, Value()});
            std::cout << "Watching '" << target << "'.\n";
        } else if (cmd == "unwatch") {
            std::string target = parseArg(ss, true);
            for (auto it = watchedVars.begin(); it != watchedVars.end(); ++it) {
                if (it->targetStr == target) {
                    watchedVars.erase(it);
                    break;
                }
            }
        } else if (cmd == "clearwatch") {
            watchedVars.clear();
        } else if (cmd == "flag") {
            Scratch::greenFlagClicked();
        } else if (cmd == "stop") {
            Scratch::stopClicked();
        }
    } else {
        queueMutex.unlock();
    }

    for (auto &w : watchedVars) {
        size_t colon = w.targetStr.find(':');
        Value current;
        if (colon == std::string::npos) {
            if (Scratch::stageSprite) {
                for (auto &[id, v] : Scratch::stageSprite->variables) {
                    if (v.name == w.targetStr) {
                        current = v.value;
                        break;
                    }
                }
            }
        } else {
            std::string spriteName = w.targetStr.substr(0, colon), varName = w.targetStr.substr(colon + 1);
            Sprite *s = findSprite(spriteName);
            if (s) {
                for (auto &[id, v] : s->variables) {
                    if (v.name == varName) {
                        current = v.value;
                        break;
                    }
                }
            }
        }
        if (!current.asString().empty() && current.asString() != w.lastValue.asString()) {
            std::cout << "[WATCH] " << w.targetStr << " : " << w.lastValue.asString() << " -> " << current.asString() << "\n";
            w.lastValue = current;
        }
    }
}

} // namespace Inspector

#else

namespace Inspector {
void init() {}
void processCommands() {}
} // namespace Inspector

#endif
