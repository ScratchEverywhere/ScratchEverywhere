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

    extension->luaState.new_usertype<ScriptThread>("ScriptThread",
                                                   "callStack", &ScriptThread::callStack,
                                                   "eraseState", &ScriptThread::eraseState,
                                                   "getState", &ScriptThread::getState,
                                                   "clear", &ScriptThread::clear,
                                                   "isRecursiveProcedureCall", &ScriptThread::isRecursiveProcedureCall,
                                                   "MyBlocksVariablen", &ScriptThread::MyBlocksVariablen,
                                                   "blockHat", &ScriptThread::blockHat,
                                                   "finished", &ScriptThread::finished,
                                                   "id", &ScriptThread::id,
                                                   "withoutScreenRefresh", &ScriptThread::withoutScreenRefresh,
                                                   "states", &ScriptThread::states,
                                                   "sprite", &ScriptThread::sprite,
                                                   "returnValue", &ScriptThread::returnValue,
                                                   "nextBlock", &ScriptThread::nextBlock);

    extension->luaState.new_usertype<Sprite>("Sprite",
                                             "shouldDoSpriteClick", &Sprite::shouldDoSpriteClick,
                                             "spriteHeight", &Sprite::spriteHeight,
                                             "spriteWidth", &Sprite::spriteWidth,
                                             "brightnessEffect", &Sprite::brightnessEffect,
                                             "colorEffect", &Sprite::colorEffect,
                                             "yPosition", &Sprite::yPosition,
                                             "xPosition", &Sprite::xPosition,
                                             "volume", &Sprite::volume,
                                             "broadcasts", &Sprite::broadcasts,
                                             "sounds", &Sprite::sounds,
                                             "toDelete", &Sprite::toDelete,
                                             "textToSpeechData", &Sprite::textToSpeechData,
                                             "size", &Sprite::size,
                                             "rotationStyle", &Sprite::rotationStyle,
                                             "rotation", &Sprite::rotation,
                                             "collisionPoints", &Sprite::collisionPoints,
                                             "pitch", &Sprite::pitch,
                                             "penData", &Sprite::penData,
                                             "costumes", &Sprite::costumes,
                                             "currentCostume", &Sprite::currentCostume,
                                             "list", &Sprite::lists,
                                             "pan", &Sprite::pan,
                                             "variables", &Sprite::variables,
                                             "name", &Sprite::name,
                                             "instrument", &Sprite::instrument,
                                             "customHatBlock", &Sprite::customHatBlock,
                                             "draggable", &Sprite::draggable,
                                             "renderInfo", &Sprite::renderInfo,
                                             "layer", &Sprite::layer,
                                             "isStage", &Sprite::isStage,
                                             "isClone", &Sprite::isClone,
                                             "hats", &Sprite::hats,
                                             "ghostEffect", &Sprite::ghostEffect);

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

    extension->luaState.new_usertype<Block>("Block",
                                            "nextBlock", &Block::nextBlock,
                                            "argumentNames", &Block::argumentNames,
                                            "hasReturnValue", &Block::hasReturnValue,
                                            "shadow", &Block::shadow,
                                            "argumentIDs", &Block::argumentIDs,
                                            "argumentDefaults", &Block::argumentDefaults,
                                            "MyBlockDefinitionID", &Block::MyBlockDefinitionID,
                                            "opcode", &Block::opcode,
                                            "inputs", &Block::inputs,
                                            "fields", &Block::fields,
                                            "MyBlockWithoutScreenRefresh", &Block::MyBlockWithoutScreenRefresh,
                                            "blockFunction", &Block::blockFunction,
                                            "isEndBlock", &Block::isEndBlock,
                                            "isEndBlock", &Block::isEndBlock);

    extension->luaState.new_usertype<ParsedInput>("ParsedInput",
                                                  "block", &ParsedInput::block,
                                                  "inputType", &ParsedInput::inputType,
                                                  "variableId", &ParsedInput::variableId,
                                                  "value", &ParsedInput::value,
                                                  "list", &ParsedInput::list,
                                                  "variable", &ParsedInput::variable,
                                                  "calculated", &ParsedInput::calculated);

    extension->luaState.new_enum<ParsedInput::InputType>("InputType", {{"Block", ParsedInput::InputType::BLOCK},
                                                                       {"Variable", ParsedInput::InputType::VARIABLE},
                                                                       {"Value", ParsedInput::InputType::VALUE}});

    extension->luaState.new_usertype<ParsedField>("ParsedField", "id", &ParsedField::id, "value", &ParsedField::value);

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

    extension->luaState["runtime"] = extension->luaState.create_table();

    extension->luaState["runtime"]["getThread"] = []() { return currentThread; };
    extension->luaState["runtime"]["getSprite"] = []() { return currentSprite; };
    extension->luaState["runtime"]["getBlock"] = []() { return currentBlock; };

    extension->luaState["runtime"].set_function("getVariableValue", [](std::string variableId, sol::optional<Sprite *> sprite) {
        return BlockExecutor::getVariableValue(variableId, sprite.value_or(currentSprite));
    });
    extension->luaState["runtime"].set_function("setVariableValue", [](std::string variableId, sol::object value, sol::optional<Sprite *> sprite) {
        return BlockExecutor::setVariableValue(variableId, objectToValue(value), sprite.value_or(currentSprite));
    });
    extension->luaState["runtime"].set_function("getListItems", sol::overload([]() { return std::ref(*Scratch::getListItems(*currentBlock, currentSprite)); }, [](Block &block, Sprite *sprite) { return std::ref(*Scratch::getListItems(block, sprite)); }));
}
