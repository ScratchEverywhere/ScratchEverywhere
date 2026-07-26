#include "runtime.hpp"
#include "blockExecutor.hpp"
#include "interface.hpp"
#include "meta.hpp"
#include "sprite.hpp"
#include <runtime.hpp>
#include <sol/forward.hpp>
#include <sol/sol.hpp>

static ScriptThread *currentThread = nullptr;
static Sprite *currentSprite = nullptr;
static Block *currentBlock = nullptr;

void extensions::runtime::setThread(ScriptThread *thread) {
    currentThread = thread;
}

void extensions::runtime::setSprite(Sprite *sprite) {
    currentSprite = sprite;
}

void extensions::runtime::setBlock(Block *block) {
    currentBlock = block;
}

void extensions::runtime::clearData() {
    currentThread = nullptr;
    currentSprite = nullptr;
    currentBlock = nullptr;
}

void extensions::runtime::registerAPI(Extension *extension) {
    if (!extension->hasPermission(ExtensionPermission::RUNTIME)) return;

    extension->luaState.new_enum<BlockResult>("BlockResult", {{"Continue", BlockResult::CONTINUE},
                                                              {"ContinueImmediately", BlockResult::CONTINUE_IMMEDIATELY},
                                                              {"Repeat", BlockResult::REPEAT},
                                                              {"Return", BlockResult::RETURN}});

    extension->luaState.new_usertype<RenderInfo>("RenderInfo",
                                                 "renderRotation", &RenderInfo::renderRotation,
                                                 "renderScaleX", &RenderInfo::renderScaleX,
                                                 "renderScaleY", &RenderInfo::renderScaleY,
                                                 "renderX", &RenderInfo::renderX,
                                                 "renderY", &RenderInfo::renderY,
                                                 "oldX", &RenderInfo::oldX,
                                                 "oldY", &RenderInfo::oldY,
                                                 "oldCostumeID", &RenderInfo::oldCostumeID,
                                                 "oldRotation", &RenderInfo::oldRotation,
                                                 "oldSize", &RenderInfo::oldSize,
                                                 "forceUpdate", &RenderInfo::forceUpdate);

    extension->luaState.new_usertype<BlockState>("BlockState",
                                                 "myBlockThread", &BlockState::myBlockThread,
                                                 "completedSteps", &BlockState::completedSteps,
                                                 "threads", &BlockState::threads,
                                                 "glideEndX", &BlockState::glideEndX,
                                                 "glideEndY", &BlockState::glideEndY,
                                                 "glideStartX", &BlockState::glideStartX,
                                                 "glideStartY", &BlockState::glideStartY,
                                                 "clear", &BlockState::clear,
                                                 "waitTimer", &BlockState::waitTimer,
                                                 "waitDuration", &BlockState::waitDuration,
                                                 "repeatTimes", &BlockState::repeatTimes,
                                                 "name", &BlockState::name,
                                                 "musicChannel", &BlockState::musicChannel);

    // wtf clang-format, what is this
    extension->luaState.new_usertype<ScriptThread>("ScriptThread", "id", &ScriptThread::id, "sprite", &ScriptThread::sprite, "blockHat", &ScriptThread::blockHat, "nextBlock", &ScriptThread::nextBlock, "finished", &ScriptThread::finished, "withoutScreenRefresh", &ScriptThread::withoutScreenRefresh, "returnValue", &ScriptThread::returnValue, "callStack", &ScriptThread::callStack, "eraseState", &ScriptThread::eraseState, "getState", &ScriptThread::getState, "clear", &ScriptThread::clear, "isRecursiveProcedureCall", &ScriptThread::isRecursiveProcedureCall, "getStateForBlock", [](ScriptThread &s, Block *b) -> BlockState * { auto it = s.states.find(b); return (it != s.states.end()) ? it->second : nullptr; }, "setStateForBlock", [](ScriptThread &s, Block *b, BlockState *state) { s.states[b] = state; }, "getMyBlocksVariable", [](ScriptThread &s, const std::string &key) -> sol::optional<Value> { auto it = s.MyBlocksVariablen.find(key); if (it != s.MyBlocksVariablen.end()) return it->second; return sol::nullopt; }, "setMyBlocksVariable", [](ScriptThread &s, const std::string &key, Value val) { s.MyBlocksVariablen[key] = val; });

    // AND THIS
    extension->luaState.new_usertype<Sprite>("Sprite", "name", &Sprite::name, "isStage", &Sprite::isStage, "draggable", &Sprite::draggable, "visible", &Sprite::visible, "isClone", &Sprite::isClone, "toDelete", &Sprite::toDelete, "shouldDoSpriteClick", &Sprite::shouldDoSpriteClick, "currentCostume", &Sprite::currentCostume, "xPosition", &Sprite::xPosition, "yPosition", &Sprite::yPosition, "size", &Sprite::size, "rotation", &Sprite::rotation, "layer", &Sprite::layer, "renderInfo", &Sprite::renderInfo, "instrument", &Sprite::instrument, "ghostEffect", &Sprite::ghostEffect, "brightnessEffect", &Sprite::brightnessEffect, "colorEffect", &Sprite::colorEffect, "volume", &Sprite::volume, "pitch", &Sprite::pitch, "pan", &Sprite::pan, "rotationStyle", &Sprite::rotationStyle, "collisionPoints", &Sprite::collisionPoints, "spriteWidth", &Sprite::spriteWidth, "spriteHeight", &Sprite::spriteHeight, "sounds", &Sprite::sounds, "costumes", &Sprite::costumes, "getVariable", [](Sprite &s, const std::string &id) -> sol::optional<Variable> { auto it = s.variables.find(id); if (it != s.variables.end()) return it->second;return sol::nullopt; }, "setVariable", [](Sprite &s, const std::string &id, Variable var) { s.variables[id] = var; }, "getList", [](Sprite &s, const std::string &id) -> sol::optional<List> {auto it = s.lists.find(id);if (it != s.lists.end()) return it->second;return sol::nullopt; }, "setList", [](Sprite &s, const std::string &id, List lst) { s.lists[id] = lst; }, "getBroadcast", [](Sprite &s, const std::string &id) -> sol::optional<Broadcast> {auto it = s.broadcasts.find(id);if (it != s.broadcasts.end()) return it->second;return sol::nullopt; }, "getCustomHatBlock", [](Sprite &s, const std::string &key) -> Block * {auto it = s.customHatBlock.find(key);return (it != s.customHatBlock.end()) ? it->second : nullptr; });

    extension->luaState.new_enum<Sprite::RotationStyle>("RotationStyle", {{"None", Sprite::RotationStyle::NONE},
                                                                          {"AllAround", Sprite::RotationStyle::ALL_AROUND},
                                                                          {"LeftRight", Sprite::RotationStyle::LEFT_RIGHT}});

    extension->luaState.new_usertype<Variable>("Variable",
                                               "id", &Variable::id,
                                               "name", &Variable::name,
                                               "value", &Variable::value);

    extension->luaState.new_usertype<List>("List",
                                           "id", &List::id,
                                           "name", &List::name,
                                           "items", &List::items);

    // i hate u
    extension->luaState.new_usertype<Block>("Block", "nextBlock", &Block::nextBlock, "argumentNames", &Block::argumentNames, "hasReturnValue", &Block::hasReturnValue, "shadow", &Block::shadow, "argumentIDs", &Block::argumentIDs, "argumentDefaults", &Block::argumentDefaults, "MyBlockDefinitionID", &Block::MyBlockDefinitionID, "opcode", &Block::opcode, "MyBlockWithoutScreenRefresh", &Block::MyBlockWithoutScreenRefresh, "isEndBlock", &Block::isEndBlock, "getInput", [](Block &b, const std::string &key) -> sol::optional<ParsedInput> {const auto* i = Scratch::getInput(&b,key);if(i != nullptr) return *i;return sol::nullopt; }, "getField", [](Block &b, const std::string &key) -> sol::optional<ParsedField> {const auto* f = Scratch::getField(b,key); if(f != nullptr) return *f;return sol::nullopt; });

    extension->luaState.new_usertype<ParsedInput>("ParsedInput",
                                                  "block", &ParsedInput::block,
                                                  "inputType", &ParsedInput::inputType,
                                                  "variableId", &ParsedInput::variableId,
                                                  "value", &ParsedInput::value,
                                                  "list", &ParsedInput::list,
                                                  "calculated", &ParsedInput::calculated
#ifdef ENABLE_CACHING
                                                  ,
                                                  "variable", &ParsedInput::variable
#endif
    );

    extension->luaState.new_enum<ParsedInput::InputType>("InputType", {{"Value", ParsedInput::InputType::VALUE},
                                                                       {"Variable", ParsedInput::InputType::VARIABLE},
                                                                       {"Block", ParsedInput::InputType::BLOCK}});

    extension->luaState.new_usertype<ParsedField>("ParsedField",
                                                  "id", &ParsedField::id,
                                                  "value", &ParsedField::value);

    extension->luaState.new_usertype<Sound>("Sound",
                                            "id", &Sound::id,
                                            "dataFormat", &Sound::dataFormat,
                                            "fullName", &Sound::fullName,
                                            "name", &Sound::name,
                                            "sampleRate", &Sound::sampleRate,
                                            "sampleCount", &Sound::sampleCount);

    extension->luaState.new_usertype<Costume>("Costume",
                                              "id", &Costume::id,
                                              "bitmapResolution", &Costume::bitmapResolution,
                                              "bitmask", &Costume::bitmask,
                                              "rotationCenterX", &Costume::rotationCenterX,
                                              "rotationCenterY", &Costume::rotationCenterY,
                                              "isSVG", &Costume::isSVG,
                                              "name", &Costume::name,
                                              "fullName", &Costume::fullName,
                                              "dataFormat", &Costume::dataFormat);

    extension->luaState.new_usertype<Bitmask>("Bitmask",
                                              "getPixel", &Bitmask::getPixel,
                                              "bits", &Bitmask::bits,
                                              "width", &Bitmask::width,
                                              "height", &Bitmask::height,
                                              "maxRadius", &Bitmask::maxRadius,
                                              "scaleFactor", &Bitmask::scaleFactor);

    extension->luaState["runtime"] = extension->luaState.create_table();

    extension->luaState["runtime"]["getThread"] = []() { return currentThread; };
    extension->luaState["runtime"]["getSprite"] = []() { return currentSprite; };
    extension->luaState["runtime"]["getBlock"] = []() { return currentBlock; };

    extension->luaState["runtime"]["getVariableValue"] = [](std::string variableId, Sprite *sprite) {
        return BlockExecutor::getVariableValue(variableId, sprite ? sprite : currentSprite);
    };

    extension->luaState["runtime"]["setVariableValue"] = [](std::string variableId, sol::object value, Sprite *sprite) {
        return BlockExecutor::setVariableValue(variableId, objectToValue(value), sprite ? sprite : currentSprite);
    };

    extension->luaState["runtime"]["getListItems"] = sol::overload(
        []() { return std::ref(*Scratch::getListItems(*currentBlock, currentSprite)); },
        [](Block &block, Sprite *sprite) { return std::ref(*Scratch::getListItems(block, sprite)); });
}
