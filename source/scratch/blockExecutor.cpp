#include "blockExecutor.hpp"
#include "blocks/control.hpp"
#include "blocks/data.hpp"
#include "blocks/events.hpp"
#include "blocks/looks.hpp"
#include "blocks/makeymakey.hpp"
#include "blocks/motion.hpp"
#include "blocks/operator.hpp"
#include "blocks/pen.hpp"
#include "blocks/procedure.hpp"
#include "blocks/sensing.hpp"
#include "blocks/sound.hpp"
#include "blocks/text2speech.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "os.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <ratio>
#include <utility>
#include <vector>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

size_t blocksRun = 0;
Timer BlockExecutor::timer;

BlockExecutor::BlockExecutor() {
    registerHandlers();
}

void BlockExecutor::registerHandlers() {

    handlers = {
        {"motion_movesteps", MotionBlocks::moveSteps},
        {"motion_gotoxy", MotionBlocks::goToXY},
        {"motion_goto", MotionBlocks::goTo},
        {"motion_changexby", MotionBlocks::changeXBy},
        {"motion_changeyby", MotionBlocks::changeYBy},
        {"motion_setx", MotionBlocks::setX},
        {"motion_sety", MotionBlocks::setY},
        {"motion_glidesecstoxy", MotionBlocks::glideSecsToXY},
        {"motion_glideto", MotionBlocks::glideTo},
        {"motion_turnright", MotionBlocks::turnRight},
        {"motion_turnleft", MotionBlocks::turnLeft},
        {"motion_pointindirection", MotionBlocks::pointInDirection},
        {"motion_pointtowards", MotionBlocks::pointToward},
        {"motion_setrotationstyle", MotionBlocks::setRotationStyle},
        {"motion_ifonedgebounce", MotionBlocks::ifOnEdgeBounce},

        {"looks_show", LooksBlocks::show},
        {"looks_hide", LooksBlocks::hide},
        {"looks_switchcostumeto", LooksBlocks::switchCostumeTo},
        {"looks_nextcostume", LooksBlocks::nextCostume},
        {"looks_switchbackdropto", LooksBlocks::switchBackdropTo},
        {"looks_nextbackdrop", LooksBlocks::nextBackdrop},
        {"looks_goforwardbackwardlayers", LooksBlocks::goForwardBackwardLayers},
        {"looks_gotofrontback", LooksBlocks::goToFrontBack},
        {"looks_setsizeto", LooksBlocks::setSizeTo},
        {"looks_changesizeby", LooksBlocks::changeSizeBy},
        {"looks_seteffectto", LooksBlocks::setEffectTo},
        {"looks_changeeffectby", LooksBlocks::changeEffectBy},
        {"looks_cleargraphiceffects", LooksBlocks::clearGraphicEffects},
        {"looks_say", LooksBlocks::say},
        {"looks_sayforsecs", LooksBlocks::sayForSeconds},
        {"looks_think", LooksBlocks::think},
        {"looks_thinkforsecs", LooksBlocks::thinkForSeconds},

        {"sound_play", SoundBlocks::playSound},
        {"sound_playuntildone", SoundBlocks::playSoundUntilDone},
        {"sound_stopallsounds", SoundBlocks::stopAllSounds},
        {"sound_changeeffectby", SoundBlocks::changeEffectBy},
        {"sound_seteffectto", SoundBlocks::setEffectTo},
        {"sound_cleareffects", SoundBlocks::clearSoundEffects},
        {"sound_changevolumeby", SoundBlocks::changeVolumeBy},
        {"sound_setvolumeto", SoundBlocks::setVolumeTo},

        {"event_whenflagclicked", EventBlocks::flagClicked},
        {"event_broadcast", EventBlocks::broadcast},
        {"event_broadcastandwait", EventBlocks::broadcastAndWait},
        {"event_whenkeypressed", EventBlocks::whenKeyPressed},
        {"event_whenbackdropswitchesto", EventBlocks::whenBackdropSwitchesTo},

        {"control_if", ControlBlocks::If},
        {"control_if_else", ControlBlocks::ifElse},
        {"control_create_clone_of", ControlBlocks::createCloneOf},
        {"control_delete_this_clone", ControlBlocks::deleteThisClone},
        {"control_stop", ControlBlocks::stop},
        {"control_start_as_clone", ControlBlocks::startAsClone},
        {"control_wait", ControlBlocks::wait},
        {"control_wait_until", ControlBlocks::waitUntil},
        {"control_repeat", ControlBlocks::repeat},
        {"control_repeat_until", ControlBlocks::repeatUntil},
        {"control_while", ControlBlocks::While},
        {"control_forever", ControlBlocks::forever},
        {"control_clear_counter", ControlBlocks::clearCounter},
        {"control_incr_counter", ControlBlocks::incrementCounter},
        {"control_for_each", ControlBlocks::forEach},

        {"sensing_askandwait", SensingBlocks::askAndWait},
        {"sensing_resettimer", SensingBlocks::resetTimer},
        {"sensing_setdragmode", SensingBlocks::setDragMode},

        {"data_setvariableto", DataBlocks::setVariable},
        {"data_changevariableby", DataBlocks::changeVariable},
        {"data_showvariable", DataBlocks::showVariable},
        {"data_hidevariable", DataBlocks::hideVariable},
        {"data_addtolist", DataBlocks::addToList},
        {"data_deleteoflist", DataBlocks::deleteFromList},
        {"data_deletealloflist", DataBlocks::deleteAllOfList},
        {"data_insertatlist", DataBlocks::insertAtList},
        {"data_replaceitemoflist", DataBlocks::replaceItemOfList},
        {"data_showlist", DataBlocks::showList},
        {"data_hidelist", DataBlocks::hideList},

        {"procedures_call", ProcedureBlocks::call},
        {"procedures_definition", ProcedureBlocks::definition},

        {"pen_clear", PenBlocks::EraseAll},
        {"pen_stamp", PenBlocks::Stamp},
        {"pen_penUp", PenBlocks::PenUp},
        {"pen_penDown", PenBlocks::PenDown},
        {"pen_setPenSizeTo", PenBlocks::SetPenSizeTo},
        {"pen_changePenSizeBy", PenBlocks::ChangePenSizeBy},
        {"pen_setPenColorToColor", PenBlocks::SetPenColorTo},
        {"pen_setPenColorParamTo", PenBlocks::SetPenOptionTo},
        {"pen_changePenColorParamBy", PenBlocks::ChangePenOptionBy},

        // Text2Speech
        {"text2speech_speakAndWait", SpeechBlocks::speakAndWait},
        {"text2speech_setVoice", SpeechBlocks::setVoiceTo},
        {"text2speech_setLanguage", SpeechBlocks::setLanguageTo},

        {"makeymakey_whenMakeyKeyPressed", MakeyMakeyBlocks::whenMakeyKeyPressed},
        {"makeymakey_whenCodePressed", MakeyMakeyBlocks::whenCodePressed},

        {"coreExample_exampleWithInlineImage", [](Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) { return BlockResult::CONTINUE; }},

    };

    valueHandlers = {
        {"motion_xposition", MotionBlocks::xPosition},
        {"motion_yposition", MotionBlocks::yPosition},
        {"motion_direction", MotionBlocks::direction},

        {"looks_size", LooksBlocks::size},
        {"looks_costumenumbername", LooksBlocks::costumeNumberName},
        {"looks_backdropnumbername", LooksBlocks::backdropNumberName},

        {"sound_volume", SoundBlocks::volume},

        {"control_get_counter", ControlBlocks::getCounter},

        {"sensing_timer", SensingBlocks::sensingTimer},
        {"sensing_of", SensingBlocks::of},
        {"sensing_mousex", SensingBlocks::mouseX},
        {"sensing_mousey", SensingBlocks::mouseY},
        {"sensing_distanceto", SensingBlocks::distanceTo},
        {"sensing_dayssince2000", SensingBlocks::daysSince2000},
        {"sensing_current", SensingBlocks::current},
        {"sensing_answer", SensingBlocks::sensingAnswer},
        {"sensing_keypressed", SensingBlocks::keyPressed},
        {"sensing_touchingobject", SensingBlocks::touchingObject},
        {"sensing_mousedown", SensingBlocks::mouseDown},
        {"sensing_username", SensingBlocks::username},

        {"data_itemoflist", DataBlocks::itemOfList},
        {"data_itemnumoflist", DataBlocks::itemNumOfList},
        {"data_lengthoflist", DataBlocks::lengthOfList},
        {"data_listcontainsitem", DataBlocks::listContainsItem},

        {"operator_add", OperatorBlocks::add},
        {"operator_subtract", OperatorBlocks::subtract},
        {"operator_multiply", OperatorBlocks::multiply},
        {"operator_divide", OperatorBlocks::divide},
        {"operator_random", OperatorBlocks::random},
        {"operator_join", OperatorBlocks::join},
        {"operator_letter_of", OperatorBlocks::letterOf},
        {"operator_length", OperatorBlocks::length},
        {"operator_mod", OperatorBlocks::mod},
        {"operator_round", OperatorBlocks::round},
        {"operator_mathop", OperatorBlocks::mathOp},
        {"operator_equals", OperatorBlocks::equals},
        {"operator_gt", OperatorBlocks::greaterThan},
        {"operator_lt", OperatorBlocks::lessThan},
        {"operator_and", OperatorBlocks::and_},
        {"operator_or", OperatorBlocks::or_},
        {"operator_not", OperatorBlocks::not_},
        {"operator_contains", OperatorBlocks::contains},

        {"argument_reporter_string_number", ProcedureBlocks::stringNumber},
        {"argument_reporter_boolean", ProcedureBlocks::booleanArgument},

        {"coreExample_exampleOpcode", [](Block &block, Sprite *sprite) { return Value("Stage"); }},

        {"motion_goto_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TO")); }},
        {"motion_glideto_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TO")); }},
        {"motion_pointtowards_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TOWARDS")); }},
        {"looks_costume", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "COSTUME")); }},
        {"looks_backdrops", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "BACKDROP")); }},
        {"sound_sounds_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "SOUND_MENU")); }},
        {"control_create_clone_of_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "CLONE_OPTION")); }},
        {"sensing_touchingobjectmenu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TOUCHINGOBJECTMENU")); }},
        {"sensing_distancetomenu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "DISTANCETOMENU")); }},
        {"sensing_keyoptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "KEY_OPTION")); }},
        {"sensing_of_object_menu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "OBJECT")); }},
        {"music_menu_DRUM", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "DRUM")); }},
        {"music_menu_INSTRUMENT", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "INSTRUMENT")); }},
        {"pen_menu_colorParam", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "colorParam")); }},
        {"videoSensing_menu_ATTRIBUTE", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "ATTRIBUTE")); }},
        {"videoSensing_menu_SUBJECT", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "SUBJECT")); }},
        {"videoSensing_menu_VIDEO_STATE", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "VIDEO_STATE")); }},
        {"text2speech_menu_voices", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "voices")); }},
        {"text2speech_menu_languages", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "languages")); }},
        {"translate_menu_languages", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "languages")); }},
        {"makeymakey_menu_KEY", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "KEY")); }},
        {"makeymakey_menu_SEQUENCE", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "SEQUENCE")); }},
        {"microbit_menu_buttons", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "buttons")); }},
        {"microbit_menu_gestures", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "gestures")); }},
        {"microbit_menu_tiltDirectionAny", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "tiltDirectionAny")); }},
        {"microbit_menu_tiltDirection", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "tiltDirection")); }},
        {"microbit_menu_touchPins", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "touchPins")); }},
        {"ev3_menu_motorPorts", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "motorPorts")); }},
        {"ev3_menu_sensorPorts", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "sensorPorts")); }},
        {"boost_menu_MOTOR_ID", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MOTOR_ID")); }},
        {"boost_menu_MOTOR_DIRECTION", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MOTOR_DIRECTION")); }},
        {"boost_menu_MOTOR_REPORTER_ID", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MOTOR_REPORTER_ID")); }},
        {"boost_menu_COLOR", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "COLOR")); }},
        {"boost_menu_TILT_DIRECTION_ANY", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TILT_DIRECTION_ANY")); }},
        {"boost_menu_TILT_DIRECTION", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TILT_DIRECTION")); }},
        {"wedo2_menu_MOTOR_ID", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MOTOR_ID")); }},
        {"wedo2_menu_MOTOR_DIRECTION", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MOTOR_DIRECTION")); }},
        {"wedo2_menu_OP", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "OP")); }},
        {"wedo2_menu_TILT_DIRECTION_ANY", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TILT_DIRECTION_ANY")); }},
        {"wedo2_menu_TILT_DIRECTION", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TILT_DIRECTION")); }},
        {"gdxfor_menu_gestureOptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "gestureOptions")); }},
        {"gdxfor_menu_pushPullOptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "pushPullOptions")); }},
        {"gdxfor_menu_tiltAnyOptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "tiltAnyOptions")); }},
        {"gdxfor_menu_tiltOptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "tiltOptions")); }},
        {"gdxfor_menu_axisOptions", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "axisOptions")); }},

        {"even_touchingobjectmenu", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "TOUCHINGOBJECTMENU")); }},
        {"microbit_menu_pinState", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "pinState")); }},

        {"note", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "NOTE")); }},
        {"matrix", [](Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, "MATRIX")); }},
    };
}

std::vector<Block *> BlockExecutor::runBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::vector<Block *> ranBlocks;
    auto start = std::chrono::high_resolution_clock::now();
    Block *currentBlock = &block;

    bool localWithoutRefresh = false;
    if (!withoutScreenRefresh) {
        withoutScreenRefresh = &localWithoutRefresh;
    }

    if (!sprite || sprite->toDelete) {
        return ranBlocks;
    }

    while (currentBlock && currentBlock->id != "null") {
        blocksRun += 1;
        ranBlocks.push_back(currentBlock);
        BlockResult result = executeBlock(*currentBlock, sprite, withoutScreenRefresh, fromRepeat);

        if (result == BlockResult::RETURN) {
            return ranBlocks;
        }

        // runBroadcasts();

        // Move to next block
        if (!currentBlock->next.empty()) {
            currentBlock = &sprite->blocks[currentBlock->next];
        } else {
            break;
        }
    }

    // Timing measurement
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    if (duration.count() > 0) {
        // std::cout << " took " << duration.count() << " milliseconds!" << std::endl;
    }
    return ranBlocks;
}

BlockResult BlockExecutor::executeBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    auto iterator = handlers.find(block.opcode);
    if (iterator != handlers.end()) {
        return iterator->second(block, sprite, withoutScreenRefresh, fromRepeat);
    }

    return BlockResult::CONTINUE;
}

void BlockExecutor::executeKeyHats() {
    for (const auto &key : Input::keyHeldDuration) {
        if (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), key.first) == Input::inputButtons.end()) {
            Input::keyHeldDuration[key.first] = 0;
        } else {
            Input::keyHeldDuration[key.first]++;
        }
    }

    for (std::string key : Input::inputButtons) {
        if (Input::keyHeldDuration.find(key) == Input::keyHeldDuration.end()) Input::keyHeldDuration[key] = 1;
    }

    for (std::string key : Input::inputButtons) {
        if (key != "any" && Input::keyHeldDuration[key] == 1) {
            Input::codePressedBlockOpcodes.clear();
            std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
            std::transform(addKey.begin(), addKey.end(), addKey.begin(), ::tolower);
            Input::inputBuffer.push_back(addKey);
            if (Input::inputBuffer.size() == 101) Input::inputBuffer.erase(Input::inputBuffer.begin());
        }
    }

    std::vector<Sprite *> sprToRun = sprites;
    for (Sprite *currentSprite : sprToRun) {
        for (auto &[id, data] : currentSprite->blocks) {
            if (data.opcode == "event_whenkeypressed") {
                std::string key = Scratch::getFieldValue(data, "KEY_OPTION");
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 13))
                    executor.runBlock(data, currentSprite);
            } else if (data.opcode == "makeymakey_whenMakeyKeyPressed") {
                std::string key = Input::convertToKey(Scratch::getInputValue(data, "KEY", currentSprite), true);
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && Input::keyHeldDuration.find(key)->second > 0)
                    executor.runBlock(data, currentSprite);
            }
        }
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

void BlockExecutor::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : sprites) {
            if (!sprite->visible) continue;

            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                if (Input::mousePointer.heldFrames < 2 && isColliding("mouse", sprite)) {

                    // run all "when this sprite clicked" blocks in the sprite
                    hasClicked = true;
                    for (auto &[id, data] : sprite->blocks) {
                        if (data.opcode == "event_whenthisspriteclicked") {
                            executor.runBlock(data, sprite);
                        }
                    }
                }
            }
            // start dragging a sprite
            if (Input::draggingSprite == nullptr && Input::mousePointer.heldFrames < 2 && sprite->draggable && isColliding("mouse", sprite)) {
                Input::draggingSprite = sprite;
            }
            if (hasClicked) break;
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    // move a dragging sprite
    if (Input::draggingSprite != nullptr) {
        if (Input::mousePointer.heldFrames == 0) {
            Input::draggingSprite = nullptr;
            return;
        }
        Input::draggingSprite->xPosition = Input::mousePointer.x - (Input::draggingSprite->spriteWidth / 2);
        Input::draggingSprite->yPosition = Input::mousePointer.y + (Input::draggingSprite->spriteHeight / 2);
    }
}

void BlockExecutor::runRepeatBlocks() {
    blocksRun = 0;
    bool withoutRefresh = false;

    // repeat ONLY the block most recently added to the repeat chain,,,
    std::vector<Sprite *> sprToRun = sprites;
    for (auto &sprite : sprToRun) {
        for (auto &[id, blockChain] : sprite->blockChains) {
            auto &repeatList = blockChain.blocksToRepeat;
            if (!repeatList.empty()) {
                std::string toRepeat = repeatList.back();
                if (!toRepeat.empty()) {
                    Block *toRun = &sprite->blocks[toRepeat];
                    if (toRun != nullptr) {
                        executor.runBlock(*toRun, sprite, &withoutRefresh, true);
                    }
                }
            }
        }
    }
    // delete sprites ready for deletion

    for (auto &toDelete : sprites) {
        if (!toDelete->toDelete) continue;
        for (auto &[id, block] : toDelete->blocks) {
            for (std::string repeatID : toDelete->blockChains[block.blockChainID].blocksToRepeat) {
                Block *repeatBlock = findBlock(repeatID);
                if (repeatBlock) {
                    repeatBlock->repeatTimes = -1;
                }
            }
        }
        toDelete->isDeleted = true;
    }
    sprites.erase(std::remove_if(sprites.begin(), sprites.end(),
                                 [](Sprite *s) { return s->toDelete; }),
                  sprites.end());
}

void BlockExecutor::runRepeatsWithoutRefresh(Sprite *sprite, std::string blockChainID) {
    bool withoutRefresh = true;
    if (sprite->blockChains.find(blockChainID) != sprite->blockChains.end()) {
        while (!sprite->blockChains[blockChainID].blocksToRepeat.empty()) {
            std::string toRepeat = sprite->blockChains[blockChainID].blocksToRepeat.back();
            Block *toRun = findBlock(toRepeat);
            if (toRun != nullptr)
                executor.runBlock(*toRun, sprite, &withoutRefresh, true);
        }
    }
}

BlockResult BlockExecutor::runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh) {
    for (auto &[id, data] : sprite->customBlocks) {
        if (id == block.customBlockId) {
            // Set up argument values
            for (std::string arg : data.argumentIds) {
                data.argumentValues[arg] = block.parsedInputs->find(arg) == block.parsedInputs->end() ? Value(0) : Scratch::getInputValue(block, arg, sprite);
            }

            // std::cout << "running custom block " << data.blockId << std::endl;

            // Get the parent of the prototype block (the definition containing all blocks)
            Block *customBlockDefinition = &sprite->blocks[sprite->blocks[data.blockId].parent];

            callerBlock->customBlockPtr = customBlockDefinition;

            bool localWithoutRefresh = data.runWithoutScreenRefresh;

            // If the parent chain is running without refresh, force this one to also run without refresh
            if (!localWithoutRefresh && withoutScreenRefresh != nullptr) {
                localWithoutRefresh = *withoutScreenRefresh;
            }

            // std::cout << "RWSR = " << localWithoutRefresh << std::endl;

            // Execute the custom block definition
            executor.runBlock(*customBlockDefinition, sprite, &localWithoutRefresh);

            if (localWithoutRefresh) {
                BlockExecutor::runRepeatsWithoutRefresh(sprite, customBlockDefinition->blockChainID);
            }

            break;
        }
    }

    if (block.customBlockId == "\u200B\u200Blog\u200B\u200B %s") Log::log("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Bwarn\u200B\u200B %s") Log::logWarning("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Berror\u200B\u200B %s") Log::logError("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Bopen\u200B\u200B %s .sb3") {
        Log::log("Open next Project with Block");
        Scratch::nextProject = true;
        Unzip::filePath = Scratch::getInputValue(block, "arg0", sprite).asString();
        if (Unzip::filePath.rfind("sd:", 0) == 0) {
            std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }

        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!OS::fileExists(Unzip::filePath + "/project.json"))
            Unzip::filePath = Unzip::filePath + ".sb3";

        Scratch::dataNextProject = Value();
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    if (block.customBlockId == "\u200B\u200Bopen\u200B\u200B %s .sb3 with data %s") {
        Log::log("Open next Project with Block and data");
        Scratch::nextProject = true;
        Unzip::filePath = Scratch::getInputValue(block, "arg0", sprite).asString();
        // if filepath contains sd:/ at the beginning and only at the beginning, replace it with sdmc:/
        if (Unzip::filePath.rfind("sd:", 0) == 0) {
            std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }
        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!OS::fileExists(Unzip::filePath + "/project.json"))
            Unzip::filePath = Unzip::filePath + ".sb3";

        Scratch::dataNextProject = Scratch::getInputValue(block, "arg1", sprite);
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    return BlockResult::CONTINUE;
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcast(std::string broadcastToRun) {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    // Normalize broadcast name to lowercase for case-insensitive matching
    std::transform(broadcastToRun.begin(), broadcastToRun.end(), broadcastToRun.begin(), ::tolower);

    // find all matching "when I receive" blocks
    std::vector<Sprite *> sprToRun = sprites;
    for (auto *currentSprite : sprToRun) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.opcode == "event_whenbroadcastreceived") {
                std::string receiverName = Scratch::getFieldValue(block, "BROADCAST_OPTION");
                std::transform(receiverName.begin(), receiverName.end(), receiverName.begin(), ::tolower);
                if (receiverName == broadcastToRun) {
                    blocksToRun.insert(blocksToRun.begin(), {&block, currentSprite});
                }
            }
        }
    }

    // run each matching block
    for (auto &[blockPtr, spritePtr] : blocksToRun) {
        executor.runBlock(*blockPtr, spritePtr);
    }

    return blocksToRun;
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcasts() {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    if (broadcastQueue.empty()) {
        return blocksToRun;
    }

    std::string currentBroadcast = broadcastQueue.front();
    broadcastQueue.erase(broadcastQueue.begin());

    auto results = runBroadcast(currentBroadcast);
    blocksToRun.insert(blocksToRun.end(), results.begin(), results.end());

    if (!broadcastQueue.empty()) {
        auto moreResults = runBroadcasts();
        blocksToRun.insert(blocksToRun.end(), moreResults.begin(), moreResults.end());
    }

    return blocksToRun;
}

void BlockExecutor::runAllBlocksByOpcode(std::string opcodeToFind) {
    // std::cout << "Running all " << opcodeToFind << " blocks." << "\n";
    std::vector<Block *> blocksRun;
    std::vector<Sprite *> sprToRun = sprites;
    for (Sprite *currentSprite : sprToRun) {
        for (auto &[id, data] : currentSprite->blocks) {
            if (data.opcode == opcodeToFind) {
                // runBlock(data,currentSprite);
                blocksRun.push_back(&data);
                executor.runBlock(data, currentSprite);
            }
        }
    }
}

Value BlockExecutor::getBlockValue(Block &block, Sprite *sprite) {
    auto iterator = valueHandlers.find(block.opcode);
    if (iterator != valueHandlers.end()) {
        return iterator->second(block, sprite);
    }

    return Value();
}

void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite) {
    // Set sprite variable
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        it->second.value = newValue;
        return;
    }

    // Set global variable
    auto globalIt = stageSprite->variables.find(variableId);
    if (globalIt != stageSprite->variables.end()) {
        globalIt->second.value = newValue;
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) cloudConnection->set(globalIt->second.name, globalIt->second.value.asString());
#endif
        return;
    }
}

void BlockExecutor::updateMonitors() {
    for (auto &var : Render::visibleVariables) {
        if (var.visible) {
            Sprite *sprite = nullptr;
            for (auto &spr : sprites) {
                if (var.spriteName == "" && spr->isStage) {
                    sprite = spr;
                    break;
                }
                if (spr->name == var.spriteName && !spr->isClone) {
                    sprite = spr;
                    break;
                }
            }

            if (var.opcode == "data_variable") {
                var.value = BlockExecutor::getVariableValue(var.id, sprite);
                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                // Check lists
                auto listIt = sprite->lists.find(var.id);
                if (listIt != sprite->lists.end())
                    var.list = listIt->second.items;

                // Check global lists
                auto globalIt = stageSprite->lists.find(var.id);
                if (globalIt != stageSprite->lists.end())
                    var.list = globalIt->second.items;
            } else {
                try {
                    Block newBlock;
                    newBlock.opcode = var.opcode;
                    for (const auto &[paramName, paramValue] : var.parameters) {
                        ParsedField parsedField;
                        parsedField.value = Math::removeQuotations(paramValue);
                        (*newBlock.parsedFields)[paramName] = parsedField;
                    }
                    if (var.opcode == "looks_costumenumbername")
                        var.displayName = var.spriteName + ": costume " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                    else if (var.opcode == "looks_backdropnumbername")
                        var.displayName = "backdrop " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                    else if (var.opcode == "sensing_current")
                        var.displayName = std::string(MonitorDisplayNames::getCurrentMenuMonitorName(Scratch::getFieldValue(newBlock, "CURRENTMENU")));
                    else {
                        auto spriteName = MonitorDisplayNames::getSpriteMonitorName(var.opcode);
                        if (spriteName != var.opcode) {
                            var.displayName = var.spriteName + ": " + std::string(spriteName);
                        } else {
                            auto simpleName = MonitorDisplayNames::getSimpleMonitorName(var.opcode);
                            var.displayName = simpleName != var.opcode ? std::string(simpleName) : var.opcode;
                        }
                    }
                    var.value = executor.getBlockValue(newBlock, sprite);
                } catch (...) {
                    var.value = Value("Unknown...");
                }
            }
        }
    }
}

Value BlockExecutor::getVariableValue(std::string variableId, Sprite *sprite) {
    // Check sprite variables
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        return it->second.value;
    }

    // Check lists
    auto listIt = sprite->lists.find(variableId);
    if (listIt != sprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : listIt->second.items) {
            if (item.asString().size() > 1 || !item.isString()) {
                seperator = " ";
                break;
            }
        }
        for (const auto &item : listIt->second.items) {
            result += item.asString() + seperator;
        }
        if (!result.empty() && !seperator.empty()) result.pop_back();
        Value val(result);
        return val;
    }

    // Check global variables
    auto globalIt = stageSprite->variables.find(variableId);
    if (globalIt != stageSprite->variables.end()) return globalIt->second.value;

    // Check global lists
    {
        auto globalIt = stageSprite->lists.find(variableId);
        if (globalIt != stageSprite->lists.end()) {
            std::string result;
            std::string seperator = "";
            for (const auto &item : globalIt->second.items) {
                if (item.asString().size() > 1 || !item.isString()) {
                    seperator = " ";
                    break;
                }
            }
            for (const auto &item : globalIt->second.items) {
                result += item.asString() + seperator;
            }
            if (!result.empty() && !seperator.empty()) result.pop_back();
            Value val(result);
            return val;
        }
    }

    return Value();
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    for (auto it = stageSprite->variables.begin(); it != stageSprite->variables.end(); ++it) {
        if (it->second.name == name) {
            it->second.value = Value(value);
            return;
        }
    }
}
#endif

Value BlockExecutor::getCustomBlockValue(std::string valueName, Sprite *sprite, Block block) {

    // get the parent prototype block
    Block *definitionBlock = getBlockParent(&block);
    Block *prototypeBlock = findBlock(Scratch::getInputValue(*definitionBlock, "custom_block", sprite).asString());

    for (auto &[custId, custBlock] : sprite->customBlocks) {

        // variable must be in the same custom block
        if (prototypeBlock != nullptr && custBlock.blockId != prototypeBlock->id) continue;

        auto it = std::find(custBlock.argumentNames.begin(), custBlock.argumentNames.end(), valueName);

        if (it != custBlock.argumentNames.end()) {
            size_t index = std::distance(custBlock.argumentNames.begin(), it);

            if (index < custBlock.argumentIds.size()) {
                std::string argumentId = custBlock.argumentIds[index];

                auto valueIt = custBlock.argumentValues.find(argumentId);
                if (valueIt != custBlock.argumentValues.end()) {
                    return valueIt->second;
                } else {
                    Log::logWarning("Argument ID found, but no value exists for it.");
                }
            } else {
                Log::logWarning("Index out of bounds for argumentIds!");
            }
        }
    }
    return Value();
}

void BlockExecutor::addToRepeatQueue(Sprite *sprite, Block *block) {
    auto &repeatList = sprite->blockChains[block->blockChainID].blocksToRepeat;
    if (std::find(repeatList.begin(), repeatList.end(), block->id) == repeatList.end()) {
        block->isRepeating = true;
        repeatList.push_back(block->id);
    }
}

void BlockExecutor::removeFromRepeatQueue(Sprite *sprite, Block *block) {
    auto it = sprite->blockChains.find(block->blockChainID);
    if (it != sprite->blockChains.end()) {
        auto &blocksToRepeat = it->second.blocksToRepeat;
        if (!blocksToRepeat.empty()) {
            block->isRepeating = false;
            block->repeatTimes = -1;
            blocksToRepeat.pop_back();
        }
    }
}

bool BlockExecutor::hasActiveRepeats(Sprite *sprite, std::string blockChainID) {
    if (sprite->toDelete) return false;
    if (sprite->blockChains.find(blockChainID) != sprite->blockChains.end()) {
        if (!sprite->blockChains[blockChainID].blocksToRepeat.empty()) return true;
    }
    return false;
}
