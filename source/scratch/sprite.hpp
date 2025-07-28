#pragma once
#include "value.hpp"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

class Sprite;

struct Variable {
    std::string id;
    std::string name;
    Value value;
};

struct ParsedInput {
    enum InputType {
        LITERAL,
        VARIABLE,
        BLOCK,
        BOOLEAN
    };

    InputType inputType;
    Value literalValue;
    std::string variableId;
    std::string blockId;

    ParsedInput() : inputType(LITERAL), literalValue(Value(0)) {}
};

struct Block {
    enum opCode {
        NONE,
        EVENT_WHENFLAGCLICKED,
        EVENT_WHENBROADCASTRECEIVED,
        EVENT_BROADCAST,
        EVENT_BROADCASTANDWAIT,
        EVENT_WHEN_KEY_PRESSED,
        PROCEDURES_PROTOTYPE,
        PROCEDURES_DEFINITION,
        PROCEDURES_CALL,
        ARGUMENT_REPORTER_STRING_NUMBER,
        ARGUMENT_REPORTER_BOOLEAN,
        MOTION_MOVE_STEPS,
        MOTION_POINT_TOWARD,
        MOTION_POINT_TOWARD_MENU,
        MOTION_XPOSITION,
        MOTION_YPOSITION,
        MOTION_DIRECTION,
        MOTION_GOTOXY,
        MOTION_GOTO,
        MOTION_GOTO_MENU,
        MOTION_POINTINDIRECTION,
        MOTION_TURNRIGHT,
        MOTION_TURNLEFT,
        MOTION_CHANGEXBY,
        MOTION_CHANGEYBY,
        MOTION_SETX,
        MOTION_SETY,
        MOTION_IF_ON_EDGE_BOUNCE,
        MOTION_SET_ROTATION_STYLE,
        MOTION_GLIDE_TO,
        MOTION_GLIDE_TO_MENU,
        MOTION_GLIDE_SECS_TO_XY,
        LOOKS_SIZE,
        LOOKS_SHOW,
        LOOKS_HIDE,
        LOOKS_SWITCHCOSTUMETO,
        LOOKS_COSTUME,
        LOOKS_SWITCHBACKDROPTO,
        LOOKS_BACKDROPS,
        LOOKS_NEXTBACKDROP,
        LOOKS_BACKDROPNUMBERNAME,
        LOOKS_COSTUMENUMBERNAME,
        LOOKS_NEXTCOSTUME,
        LOOKS_CHANGESIZEBY,
        LOOKS_SETSIZETO,
        LOOKS_GO_FORWARD_BACKWARD_LAYERS,
        LOOKS_GO_TO_FRONT_BACK,
        LOOKS_SET_EFFECT_TO,
        LOOKS_CHANGE_EFFECT_BY,
        LOOKS_CLEAR_GRAPHIC_EFFECTS,
        SOUND_VOLUME,
        SOUND_PLAY_UNTIL_DONE,
        SOUND_SOUNDS_MENU,
        SOUND_CLEAR_EFFECTS,
        SOUND_PLAY,
        SOUND_STOP_ALL_SOUNDS,
        SOUND_CHANGE_EFFECT_BY,
        SOUND_SET_EFFECT_TO,
        SOUND_CHANGE_VOLUME_BY,
        SOUND_SET_VOLUME_TO,
        SENSING_TIMER,
        SENSING_RESETTIMER,
        CONTROL_WAIT_UNTIL,
        CONTROL_WAIT,
        CONTROL_IF,
        CONTROL_IF_ELSE,
        CONTROL_REPEAT_UNTIL,
        CONTROL_WHILE,
        CONTROL_FOREVER,
        CONTROL_REPEAT,
        CONTROL_CREATE_CLONE_OF,
        CONTROL_CREATE_CLONE_OF_MENU,
        CONTROL_START_AS_CLONE,
        CONTROL_DELETE_THIS_CLONE,
        CONTROL_STOP,
        DATA_SETVARIABLETO,
        DATA_CHANGEVARIABLEBY,
        OPERATOR_ADD,
        OPERATOR_SUBTRACT,
        OPERATOR_MULTIPLY,
        OPERATOR_DIVIDE,
        OPERATOR_RANDOM,
        OPERATOR_JOIN,
        OPERATOR_LETTER_OF,
        OPERATOR_LENGTH,
        OPERATOR_MOD,
        OPERATOR_ROUND,
        OPERATOR_MATHOP,
        DATA_ITEMOFLIST,
        DATA_ITEMNUMOFLIST,
        DATA_LENGTHOFLIST,
        DATA_ADD_TO_LIST,
        DATA_DELETE_OF_LIST,
        DATA_DELETE_ALL_OF_LIST,
        DATA_INSERT_AT_LIST,
        DATA_REPLACE_ITEM_OF_LIST,
        DATA_LIST_CONTAINS_ITEM,
        SENSING_KEYPRESSED,
        SENSING_KEYOPTIONS,
        SENSING_OF,
        SENSING_OF_OBJECT_MENU,
        SENSING_TOUCHINGOBJECT,
        SENSING_TOUCHINGOBJECTMENU,
        SENSING_MOUSEDOWN,
        SENSING_MOUSEX,
        SENSING_MOUSEY,
        SENSING_DISTANCETO,
        SENSING_DISTANCETO_MENU,
        SENSING_DAYS_SINCE_2000,
        SENSING_CURRENT,
        SENSING_ASK_AND_WAIT,
        SENSING_ANSWER,
        SENSING_USERNAME,
        SENSING_SET_DRAG_MODE,
        OPERATOR_EQUALS,
        OPERATOR_GT,
        OPERATOR_LT,
        OPERATOR_AND,
        OPERATOR_OR,
        OPERATOR_NOT,
        OPERATOR_CONTAINS,

    };

    opCode stringToOpcode(std::string opCodeString) {
        if (opCodeString == "event_whenflagclicked") return EVENT_WHENFLAGCLICKED;
        if (opCodeString == "event_whenbroadcastreceived") return EVENT_WHENBROADCASTRECEIVED;
        if (opCodeString == "event_broadcastandwait") return EVENT_BROADCASTANDWAIT;
        if (opCodeString == "event_broadcast") return EVENT_BROADCAST;
        if (opCodeString == "event_whenkeypressed") return EVENT_WHEN_KEY_PRESSED;
        if (opCodeString == "procedures_prototype") return PROCEDURES_PROTOTYPE;
        if (opCodeString == "procedures_definition") return PROCEDURES_DEFINITION;
        if (opCodeString == "procedures_call") return PROCEDURES_CALL;
        if (opCodeString == "argument_reporter_string_number") return ARGUMENT_REPORTER_STRING_NUMBER;
        if (opCodeString == "argument_reporter_boolean") return ARGUMENT_REPORTER_BOOLEAN;
        if (opCodeString == "motion_xposition") return MOTION_XPOSITION;
        if (opCodeString == "motion_yposition") return MOTION_YPOSITION;
        if (opCodeString == "motion_direction") return MOTION_DIRECTION;
        if (opCodeString == "motion_gotoxy") return MOTION_GOTOXY;
        if (opCodeString == "motion_goto") return MOTION_GOTO;
        if (opCodeString == "motion_goto_menu") return MOTION_GOTO_MENU;
        if (opCodeString == "motion_movesteps") return MOTION_MOVE_STEPS;
        if (opCodeString == "motion_pointtowards") return MOTION_POINT_TOWARD;
        if (opCodeString == "motion_pointtowards_menu") return MOTION_POINT_TOWARD_MENU;
        if (opCodeString == "motion_pointindirection") return MOTION_POINTINDIRECTION;
        if (opCodeString == "motion_turnright") return MOTION_TURNRIGHT;
        if (opCodeString == "motion_turnleft") return MOTION_TURNLEFT;
        if (opCodeString == "motion_changexby") return MOTION_CHANGEXBY;
        if (opCodeString == "motion_changeyby") return MOTION_CHANGEYBY;
        if (opCodeString == "motion_setx") return MOTION_SETX;
        if (opCodeString == "motion_sety") return MOTION_SETY;
        if (opCodeString == "motion_setrotationstyle") return MOTION_SET_ROTATION_STYLE;
        if (opCodeString == "motion_ifonedgebounce") return MOTION_IF_ON_EDGE_BOUNCE;
        if (opCodeString == "motion_glideto") return MOTION_GLIDE_TO;
        if (opCodeString == "motion_glideto_menu") return MOTION_GLIDE_TO_MENU;
        if (opCodeString == "motion_glidesecstoxy") return MOTION_GLIDE_SECS_TO_XY;
        if (opCodeString == "looks_size") return LOOKS_SIZE;
        if (opCodeString == "looks_show") return LOOKS_SHOW;
        if (opCodeString == "looks_hide") return LOOKS_HIDE;
        if (opCodeString == "looks_switchcostumeto") return LOOKS_SWITCHCOSTUMETO;
        if (opCodeString == "looks_costume") return LOOKS_COSTUME;
        if (opCodeString == "looks_nextcostume") return LOOKS_NEXTCOSTUME;
        if (opCodeString == "looks_switchbackdropto") return LOOKS_SWITCHBACKDROPTO;
        if (opCodeString == "looks_backdrops") return LOOKS_BACKDROPS;
        if (opCodeString == "looks_nextbackdrop") return LOOKS_NEXTBACKDROP;
        if (opCodeString == "looks_backdropnumbername") return LOOKS_BACKDROPNUMBERNAME;
        if (opCodeString == "looks_costumenumbername") return LOOKS_COSTUMENUMBERNAME;
        if (opCodeString == "looks_changesizeby") return LOOKS_CHANGESIZEBY;
        if (opCodeString == "looks_setsizeto") return LOOKS_SETSIZETO;
        if (opCodeString == "looks_goforwardbackwardlayers") return LOOKS_GO_FORWARD_BACKWARD_LAYERS;
        if (opCodeString == "looks_gotofrontback") return LOOKS_GO_TO_FRONT_BACK;
        if (opCodeString == "looks_seteffectto") return LOOKS_SET_EFFECT_TO;
        if (opCodeString == "looks_changeeffectby") return LOOKS_CHANGE_EFFECT_BY;
        if (opCodeString == "looks_cleargraphiceffects") return LOOKS_CLEAR_GRAPHIC_EFFECTS;
        if (opCodeString == "sound_volume") return SOUND_VOLUME;
        if (opCodeString == "sound_playuntildone") return SOUND_PLAY_UNTIL_DONE;
        if (opCodeString == "sound_sounds_menu") return SOUND_SOUNDS_MENU;
        if (opCodeString == "sound_cleareffects") return SOUND_CLEAR_EFFECTS;
        if (opCodeString == "sound_play") return SOUND_PLAY;
        if (opCodeString == "sound_stopallsounds") return SOUND_STOP_ALL_SOUNDS;
        if (opCodeString == "sound_changeeffectby") return SOUND_CHANGE_EFFECT_BY;
        if (opCodeString == "sound_seteffectto") return SOUND_SET_EFFECT_TO;
        if (opCodeString == "sound_changevolumeby") return SOUND_CHANGE_VOLUME_BY;
        if (opCodeString == "sound_setvolumeto") return SOUND_SET_VOLUME_TO;
        if (opCodeString == "sensing_timer") return SENSING_TIMER;
        if (opCodeString == "sensing_resettimer") return SENSING_RESETTIMER;
        if (opCodeString == "control_wait_until") return CONTROL_WAIT_UNTIL;
        if (opCodeString == "control_wait") return CONTROL_WAIT;
        if (opCodeString == "control_if") return CONTROL_IF;
        if (opCodeString == "control_if_else") return CONTROL_IF_ELSE;
        if (opCodeString == "control_repeat_until") return CONTROL_REPEAT_UNTIL;
        if (opCodeString == "control_while") return CONTROL_WHILE;
        if (opCodeString == "control_forever") return CONTROL_FOREVER;
        if (opCodeString == "control_repeat") return CONTROL_REPEAT;
        if (opCodeString == "control_create_clone_of") return CONTROL_CREATE_CLONE_OF;
        if (opCodeString == "control_create_clone_of_menu") return CONTROL_CREATE_CLONE_OF_MENU;
        if (opCodeString == "control_start_as_clone") return CONTROL_START_AS_CLONE;
        if (opCodeString == "control_delete_this_clone") return CONTROL_DELETE_THIS_CLONE;
        if (opCodeString == "control_stop") return CONTROL_STOP;
        if (opCodeString == "data_setvariableto") return DATA_SETVARIABLETO;
        if (opCodeString == "data_changevariableby") return DATA_CHANGEVARIABLEBY;
        if (opCodeString == "operator_add") return OPERATOR_ADD;
        if (opCodeString == "operator_subtract") return OPERATOR_SUBTRACT;
        if (opCodeString == "operator_multiply") return OPERATOR_MULTIPLY;
        if (opCodeString == "operator_divide") return OPERATOR_DIVIDE;
        if (opCodeString == "operator_random") return OPERATOR_RANDOM;
        if (opCodeString == "operator_join") return OPERATOR_JOIN;
        if (opCodeString == "operator_letter_of") return OPERATOR_LETTER_OF;
        if (opCodeString == "operator_length") return OPERATOR_LENGTH;
        if (opCodeString == "operator_mod") return OPERATOR_MOD;
        if (opCodeString == "operator_round") return OPERATOR_ROUND;
        if (opCodeString == "operator_mathop") return OPERATOR_MATHOP;
        if (opCodeString == "data_itemoflist") return DATA_ITEMOFLIST;
        if (opCodeString == "data_itemnumoflist") return DATA_ITEMNUMOFLIST;
        if (opCodeString == "data_lengthoflist") return DATA_LENGTHOFLIST;
        if (opCodeString == "data_addtolist") return DATA_ADD_TO_LIST;
        if (opCodeString == "data_deleteoflist") return DATA_DELETE_OF_LIST;
        if (opCodeString == "data_deletealloflist") return DATA_DELETE_ALL_OF_LIST;
        if (opCodeString == "data_insertatlist") return DATA_INSERT_AT_LIST;
        if (opCodeString == "data_replaceitemoflist") return DATA_REPLACE_ITEM_OF_LIST;
        if (opCodeString == "data_listcontainsitem") return DATA_LIST_CONTAINS_ITEM;
        if (opCodeString == "sensing_keypressed") return SENSING_KEYPRESSED;
        if (opCodeString == "sensing_keyoptions") return SENSING_KEYOPTIONS;
        if (opCodeString == "sensing_of") return SENSING_OF;
        if (opCodeString == "sensing_of_object_menu") return SENSING_OF_OBJECT_MENU;
        if (opCodeString == "sensing_touchingobject") return SENSING_TOUCHINGOBJECT;
        if (opCodeString == "sensing_touchingobjectmenu") return SENSING_TOUCHINGOBJECTMENU;
        if (opCodeString == "sensing_distanceto") return SENSING_DISTANCETO;
        if (opCodeString == "sensing_distancetomenu") return SENSING_DISTANCETO_MENU;
        if (opCodeString == "sensing_mousedown") return SENSING_MOUSEDOWN;
        if (opCodeString == "sensing_mousex") return SENSING_MOUSEX;
        if (opCodeString == "sensing_mousey") return SENSING_MOUSEY;
        if (opCodeString == "sensing_dayssince2000") return SENSING_DAYS_SINCE_2000;
        if (opCodeString == "sensing_current") return SENSING_CURRENT;
        if (opCodeString == "sensing_askandwait") return SENSING_ASK_AND_WAIT;
        if (opCodeString == "sensing_answer") return SENSING_ANSWER;
        if (opCodeString == "sensing_username") return SENSING_USERNAME;
        if (opCodeString == "sensing_setdragmode") return SENSING_SET_DRAG_MODE;
        if (opCodeString == "operator_equals") return OPERATOR_EQUALS;
        if (opCodeString == "operator_gt") return OPERATOR_GT;
        if (opCodeString == "operator_lt") return OPERATOR_LT;
        if (opCodeString == "operator_and") return OPERATOR_AND;
        if (opCodeString == "operator_or") return OPERATOR_OR;
        if (opCodeString == "operator_not") return OPERATOR_NOT;
        if (opCodeString == "operator_contains") return OPERATOR_CONTAINS;
        std::cerr << "Unknown opcode: " << opCodeString << std::endl;
        return NONE;
    }

    std::string id;
    opCode opcode;
    std::string next;
    Block *nextBlock;
    std::string parent;
    std::string blockChainID;
    std::map<std::string, ParsedInput> parsedInputs;
    std::unordered_map<std::string, nlohmann::json> fields;
    std::unordered_map<std::string, nlohmann::json> mutation;
    bool shadow;
    bool topLevel;
    std::string topLevelParentBlock;

    /* variables that some blocks need*/
    int repeatTimes = -1;
    bool isRepeating = false;
    double waitDuration;
    double glideStartX, glideStartY;
    double glideEndX, glideEndY;
    std::chrono::high_resolution_clock::time_point waitStartTime;
    bool customBlockExecuted = false;
    Block *customBlockPtr = nullptr;
    std::vector<std::pair<Block *, Sprite *>> broadcastsRun;
    std::vector<std::string> substackBlocksRan;
    std::string waitingIfBlock = "";
};

struct CustomBlock {

    std::string name;
    std::string tagName;
    std::string blockId;
    std::vector<std::string> argumentIds;
    std::vector<std::string> argumentNames;
    std::vector<std::string> argumentDefaults;
    std::unordered_map<std::string, Value> argumentValues;
    bool runWithoutScreenRefresh;
};

struct List {
    std::string id;
    std::string name;
    std::vector<Value> items;
};

struct Sound {
    std::string id;
    std::string name;
    std::string dataFormat;
    std::string fullName;
    int sampleRate;
    int sampleCount;
};

struct Costume {
    std::string id;
    std::string name;
    std::string fullName;
    std::string dataFormat;
    int bitmapResolution;
    double rotationCenterX;
    double rotationCenterY;
};

struct Comment {
    std::string id;
    std::string blockId;
    std::string text;
    bool minimized;
    int x;
    int y;
    int width;
    int height;
};

struct Broadcast {
    std::string id;
    std::string name;
};

struct BlockChain {
    std::vector<Block *> blockChain;
    std::vector<std::string> blocksToRepeat;
};

class Sprite {
  public:
    std::string name;
    std::string id;
    bool isStage;
    bool draggable;
    bool visible;
    bool isClone;
    bool toDelete;
    bool isDeleted = false;
    int currentCostume;
    std::string lastCostumeId = "";
    float volume;
    double xPosition;
    double yPosition;
    int rotationCenterX;
    int rotationCenterY;
    double size;
    double rotation;
    int layer;

    float ghostEffect;
    double colorEffect = -99999;

    enum RotationStyle {
        NONE,
        LEFT_RIGHT,
        ALL_AROUND
    };

    RotationStyle rotationStyle;
    std::vector<std::pair<double, double>> collisionPoints;
    int spriteWidth;
    int spriteHeight;

    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, Block> blocks;
    std::unordered_map<std::string, List> lists;
    std::unordered_map<std::string, Sound> sounds;
    std::vector<Costume> costumes;
    std::unordered_map<std::string, Comment> comments;
    std::unordered_map<std::string, Broadcast> broadcasts;
    std::unordered_map<std::string, CustomBlock> customBlocks;
    std::unordered_map<std::string, BlockChain> blockChains;
};