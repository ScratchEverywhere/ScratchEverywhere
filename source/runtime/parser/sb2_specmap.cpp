// Stolen from scratch-vm's src/serialization/sb2_specmap.js
#include "sb2_specmap.hpp"

#include <iterator>
#include <utility>

namespace {
constexpr size_t maxArgs = 3;

struct RawArg {
    Sb2ArgSpec::Kind kind;
    const char *name;
    const char *inputOp;
    const char *variableType;
};

struct RawSpec {
    const char *opcode;
    RawArg args[maxArgs];
};

struct RawEntry {
    const char *key;
    RawSpec spec;
};

constexpr RawEntry sb2RawSpecs[] = {
    {"forward:", {"motion_movesteps", {{Sb2ArgSpec::Kind::Input, "STEPS", "math_number", ""}}}},
    {"turnRight:", {"motion_turnright", {{Sb2ArgSpec::Kind::Input, "DEGREES", "math_number", ""}}}},
    {"turnLeft:", {"motion_turnleft", {{Sb2ArgSpec::Kind::Input, "DEGREES", "math_number", ""}}}},
    {"heading:", {"motion_pointindirection", {{Sb2ArgSpec::Kind::Input, "DIRECTION", "math_angle", ""}}}},
    {"pointTowards:", {"motion_pointtowards", {{Sb2ArgSpec::Kind::Input, "TOWARDS", "motion_pointtowards_menu", ""}}}},
    {"gotoX:y:", {"motion_gotoxy", {{Sb2ArgSpec::Kind::Input, "X", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "Y", "math_number", ""}}}},
    {"gotoSpriteOrMouse:", {"motion_goto", {{Sb2ArgSpec::Kind::Input, "TO", "motion_goto_menu", ""}}}},
    {"glideSecs:toX:y:elapsed:from:", {"motion_glidesecstoxy", {{Sb2ArgSpec::Kind::Input, "SECS", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "X", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "Y", "math_number", ""}}}},
    {"changeXposBy:", {"motion_changexby", {{Sb2ArgSpec::Kind::Input, "DX", "math_number", ""}}}},
    {"xpos:", {"motion_setx", {{Sb2ArgSpec::Kind::Input, "X", "math_number", ""}}}},
    {"changeYposBy:", {"motion_changeyby", {{Sb2ArgSpec::Kind::Input, "DY", "math_number", ""}}}},
    {"ypos:", {"motion_sety", {{Sb2ArgSpec::Kind::Input, "Y", "math_number", ""}}}},
    {"bounceOffEdge", {"motion_ifonedgebounce", {}}},
    {"setRotationStyle", {"motion_setrotationstyle", {{Sb2ArgSpec::Kind::Field, "STYLE", "", ""}}}},
    {"xpos", {"motion_xposition", {}}},
    {"ypos", {"motion_yposition", {}}},
    {"heading", {"motion_direction", {}}},
    {"scrollRight", {"motion_scroll_right", {{Sb2ArgSpec::Kind::Input, "DISTANCE", "math_number", ""}}}},
    {"scrollUp", {"motion_scroll_up", {{Sb2ArgSpec::Kind::Input, "DISTANCE", "math_number", ""}}}},
    {"scrollAlign", {"motion_align_scene", {{Sb2ArgSpec::Kind::Field, "ALIGNMENT", "", ""}}}},
    {"xScroll", {"motion_xscroll", {}}},
    {"yScroll", {"motion_yscroll", {}}},
    {"say:duration:elapsed:from:", {"looks_sayforsecs", {{Sb2ArgSpec::Kind::Input, "MESSAGE", "text", ""}, {Sb2ArgSpec::Kind::Input, "SECS", "math_number", ""}}}},
    {"say:", {"looks_say", {{Sb2ArgSpec::Kind::Input, "MESSAGE", "text", ""}}}},
    {"think:duration:elapsed:from:", {"looks_thinkforsecs", {{Sb2ArgSpec::Kind::Input, "MESSAGE", "text", ""}, {Sb2ArgSpec::Kind::Input, "SECS", "math_number", ""}}}},
    {"think:", {"looks_think", {{Sb2ArgSpec::Kind::Input, "MESSAGE", "text", ""}}}},
    {"show", {"looks_show", {}}},
    {"hide", {"looks_hide", {}}},
    {"hideAll", {"looks_hideallsprites", {}}},
    {"lookLike:", {"looks_switchcostumeto", {{Sb2ArgSpec::Kind::Input, "COSTUME", "looks_costume", ""}}}},
    {"nextCostume", {"looks_nextcostume", {}}},
    {"startScene", {"looks_switchbackdropto", {{Sb2ArgSpec::Kind::Input, "BACKDROP", "looks_backdrops", ""}}}},
    {"changeGraphicEffect:by:", {"looks_changeeffectby", {{Sb2ArgSpec::Kind::Field, "EFFECT", "", ""}, {Sb2ArgSpec::Kind::Input, "CHANGE", "math_number", ""}}}},
    {"setGraphicEffect:to:", {"looks_seteffectto", {{Sb2ArgSpec::Kind::Field, "EFFECT", "", ""}, {Sb2ArgSpec::Kind::Input, "VALUE", "math_number", ""}}}},
    {"filterReset", {"looks_cleargraphiceffects", {}}},
    {"changeSizeBy:", {"looks_changesizeby", {{Sb2ArgSpec::Kind::Input, "CHANGE", "math_number", ""}}}},
    {"setSizeTo:", {"looks_setsizeto", {{Sb2ArgSpec::Kind::Input, "SIZE", "math_number", ""}}}},
    {"changeStretchBy:", {"looks_changestretchby", {{Sb2ArgSpec::Kind::Input, "CHANGE", "math_number", ""}}}},
    {"setStretchTo:", {"looks_setstretchto", {{Sb2ArgSpec::Kind::Input, "STRETCH", "math_number", ""}}}},
    {"comeToFront", {"looks_gotofrontback", {}}},
    {"goBackByLayers:", {"looks_goforwardbackwardlayers", {{Sb2ArgSpec::Kind::Input, "NUM", "math_integer", ""}}}},
    {"costumeIndex", {"looks_costumenumbername", {}}},
    {"costumeName", {"looks_costumenumbername", {}}},
    {"sceneName", {"looks_backdropnumbername", {}}},
    {"scale", {"looks_size", {}}},
    {"startSceneAndWait", {"looks_switchbackdroptoandwait", {{Sb2ArgSpec::Kind::Input, "BACKDROP", "looks_backdrops", ""}}}},
    {"nextScene", {"looks_nextbackdrop", {}}},
    {"backgroundIndex", {"looks_backdropnumbername", {}}},
    {"playSound:", {"sound_play", {{Sb2ArgSpec::Kind::Input, "SOUND_MENU", "sound_sounds_menu", ""}}}},
    {"doPlaySoundAndWait", {"sound_playuntildone", {{Sb2ArgSpec::Kind::Input, "SOUND_MENU", "sound_sounds_menu", ""}}}},
    {"stopAllSounds", {"sound_stopallsounds", {}}},
    {"playDrum", {"music_playDrumForBeats", {{Sb2ArgSpec::Kind::Input, "DRUM", "music_menu_DRUM", ""}, {Sb2ArgSpec::Kind::Input, "BEATS", "math_number", ""}}}},
    {"drum:duration:elapsed:from:", {"music_midiPlayDrumForBeats", {{Sb2ArgSpec::Kind::Input, "DRUM", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "BEATS", "math_number", ""}}}},
    {"rest:elapsed:from:", {"music_restForBeats", {{Sb2ArgSpec::Kind::Input, "BEATS", "math_number", ""}}}},
    {"noteOn:duration:elapsed:from:", {"music_playNoteForBeats", {{Sb2ArgSpec::Kind::Input, "NOTE", "note", ""}, {Sb2ArgSpec::Kind::Input, "BEATS", "math_number", ""}}}},
    {"instrument:", {"music_setInstrument", {{Sb2ArgSpec::Kind::Input, "INSTRUMENT", "music_menu_INSTRUMENT", ""}}}},
    {"midiInstrument:", {"music_midiSetInstrument", {{Sb2ArgSpec::Kind::Input, "INSTRUMENT", "math_number", ""}}}},
    {"changeVolumeBy:", {"sound_changevolumeby", {{Sb2ArgSpec::Kind::Input, "VOLUME", "math_number", ""}}}},
    {"setVolumeTo:", {"sound_setvolumeto", {{Sb2ArgSpec::Kind::Input, "VOLUME", "math_number", ""}}}},
    {"volume", {"sound_volume", {}}},
    {"changeTempoBy:", {"music_changeTempo", {{Sb2ArgSpec::Kind::Input, "TEMPO", "math_number", ""}}}},
    {"setTempoTo:", {"music_setTempo", {{Sb2ArgSpec::Kind::Input, "TEMPO", "math_number", ""}}}},
    {"tempo", {"music_getTempo", {}}},
    {"clearPenTrails", {"pen_clear", {}}},
    {"stampCostume", {"pen_stamp", {}}},
    {"putPenDown", {"pen_penDown", {}}},
    {"putPenUp", {"pen_penUp", {}}},
    {"penColor:", {"pen_setPenColorToColor", {{Sb2ArgSpec::Kind::Input, "COLOR", "colour_picker", ""}}}},
    {"changePenHueBy:", {"pen_changePenHueBy", {{Sb2ArgSpec::Kind::Input, "HUE", "math_number", ""}}}},
    {"setPenHueTo:", {"pen_setPenHueToNumber", {{Sb2ArgSpec::Kind::Input, "HUE", "math_number", ""}}}},
    {"changePenShadeBy:", {"pen_changePenShadeBy", {{Sb2ArgSpec::Kind::Input, "SHADE", "math_number", ""}}}},
    {"setPenShadeTo:", {"pen_setPenShadeToNumber", {{Sb2ArgSpec::Kind::Input, "SHADE", "math_number", ""}}}},
    {"changePenSizeBy:", {"pen_changePenSizeBy", {{Sb2ArgSpec::Kind::Input, "SIZE", "math_number", ""}}}},
    {"penSize:", {"pen_setPenSizeTo", {{Sb2ArgSpec::Kind::Input, "SIZE", "math_number", ""}}}},
    {"senseVideoMotion", {"videoSensing_videoOn", {{Sb2ArgSpec::Kind::Input, "ATTRIBUTE", "videoSensing_menu_ATTRIBUTE", ""}, {Sb2ArgSpec::Kind::Input, "SUBJECT", "videoSensing_menu_SUBJECT", ""}}}},
    {"whenGreenFlag", {"event_whenflagclicked", {}}},
    {"whenKeyPressed", {"event_whenkeypressed", {{Sb2ArgSpec::Kind::Field, "KEY_OPTION", "", ""}}}},
    {"whenClicked", {"event_whenthisspriteclicked", {}}},
    {"whenSceneStarts", {"event_whenbackdropswitchesto", {{Sb2ArgSpec::Kind::Field, "BACKDROP", "", ""}}}},
    {"whenIReceive", {"event_whenbroadcastreceived", {{Sb2ArgSpec::Kind::Field, "BROADCAST_OPTION", "", "broadcast_msg"}}}},
    {"broadcast:", {"event_broadcast", {{Sb2ArgSpec::Kind::Input, "BROADCAST_INPUT", "event_broadcast_menu", "broadcast_msg"}}}},
    {"doBroadcastAndWait", {"event_broadcastandwait", {{Sb2ArgSpec::Kind::Input, "BROADCAST_INPUT", "event_broadcast_menu", "broadcast_msg"}}}},
    {"wait:elapsed:from:", {"control_wait", {{Sb2ArgSpec::Kind::Input, "DURATION", "math_positive_number", ""}}}},
    {"doRepeat", {"control_repeat", {{Sb2ArgSpec::Kind::Input, "TIMES", "math_whole_number", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"doForever", {"control_forever", {{Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"doIf", {"control_if", {{Sb2ArgSpec::Kind::Input, "CONDITION", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"doIfElse", {"control_if_else", {{Sb2ArgSpec::Kind::Input, "CONDITION", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK2", "substack", ""}}}},
    {"doWaitUntil", {"control_wait_until", {{Sb2ArgSpec::Kind::Input, "CONDITION", "boolean", ""}}}},
    {"doUntil", {"control_repeat_until", {{Sb2ArgSpec::Kind::Input, "CONDITION", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"doWhile", {"control_while", {{Sb2ArgSpec::Kind::Input, "CONDITION", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"doForLoop", {"control_for_each", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}, {Sb2ArgSpec::Kind::Input, "VALUE", "text", ""}, {Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"stopScripts", {"control_stop", {{Sb2ArgSpec::Kind::Field, "STOP_OPTION", "", ""}}}},
    {"whenCloned", {"control_start_as_clone", {}}},
    {"createCloneOf", {"control_create_clone_of", {{Sb2ArgSpec::Kind::Input, "CLONE_OPTION", "control_create_clone_of_menu", ""}}}},
    {"deleteClone", {"control_delete_this_clone", {}}},
    {"COUNT", {"control_get_counter", {}}},
    {"INCR_COUNT", {"control_incr_counter", {}}},
    {"CLR_COUNT", {"control_clear_counter", {}}},
    {"warpSpeed", {"control_all_at_once", {{Sb2ArgSpec::Kind::Input, "SUBSTACK", "substack", ""}}}},
    {"touching:", {"sensing_touchingobject", {{Sb2ArgSpec::Kind::Input, "TOUCHINGOBJECTMENU", "sensing_touchingobjectmenu", ""}}}},
    {"touchingColor:", {"sensing_touchingcolor", {{Sb2ArgSpec::Kind::Input, "COLOR", "colour_picker", ""}}}},
    {"color:sees:", {"sensing_coloristouchingcolor", {{Sb2ArgSpec::Kind::Input, "COLOR", "colour_picker", ""}, {Sb2ArgSpec::Kind::Input, "COLOR2", "colour_picker", ""}}}},
    {"distanceTo:", {"sensing_distanceto", {{Sb2ArgSpec::Kind::Input, "DISTANCETOMENU", "sensing_distancetomenu", ""}}}},
    {"doAsk", {"sensing_askandwait", {{Sb2ArgSpec::Kind::Input, "QUESTION", "text", ""}}}},
    {"answer", {"sensing_answer", {}}},
    {"keyPressed:", {"sensing_keypressed", {{Sb2ArgSpec::Kind::Input, "KEY_OPTION", "sensing_keyoptions", ""}}}},
    {"mousePressed", {"sensing_mousedown", {}}},
    {"mouseX", {"sensing_mousex", {}}},
    {"mouseY", {"sensing_mousey", {}}},
    {"soundLevel", {"sensing_loudness", {}}},
    {"isLoud", {"sensing_loud", {}}},
    {"setVideoState", {"videoSensing_videoToggle", {{Sb2ArgSpec::Kind::Input, "VIDEO_STATE", "videoSensing_menu_VIDEO_STATE", ""}}}},
    {"setVideoTransparency", {"videoSensing_setVideoTransparency", {{Sb2ArgSpec::Kind::Input, "TRANSPARENCY", "math_number", ""}}}},
    {"timer", {"sensing_timer", {}}},
    {"timerReset", {"sensing_resettimer", {}}},
    {"getAttribute:of:", {"sensing_of", {{Sb2ArgSpec::Kind::Field, "PROPERTY", "", ""}, {Sb2ArgSpec::Kind::Input, "OBJECT", "sensing_of_object_menu", ""}}}},
    {"timeAndDate", {"sensing_current", {{Sb2ArgSpec::Kind::Field, "CURRENTMENU", "", ""}}}},
    {"timestamp", {"sensing_dayssince2000", {}}},
    {"getUserName", {"sensing_username", {}}},
    {"getUserId", {"sensing_userid", {}}},
    {"+", {"operator_add", {{Sb2ArgSpec::Kind::Input, "NUM1", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "NUM2", "math_number", ""}}}},
    {"-", {"operator_subtract", {{Sb2ArgSpec::Kind::Input, "NUM1", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "NUM2", "math_number", ""}}}},
    {"*", {"operator_multiply", {{Sb2ArgSpec::Kind::Input, "NUM1", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "NUM2", "math_number", ""}}}},
    {"/", {"operator_divide", {{Sb2ArgSpec::Kind::Input, "NUM1", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "NUM2", "math_number", ""}}}},
    {"randomFrom:to:", {"operator_random", {{Sb2ArgSpec::Kind::Input, "FROM", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "TO", "math_number", ""}}}},
    {"<", {"operator_lt", {{Sb2ArgSpec::Kind::Input, "OPERAND1", "text", ""}, {Sb2ArgSpec::Kind::Input, "OPERAND2", "text", ""}}}},
    {"=", {"operator_equals", {{Sb2ArgSpec::Kind::Input, "OPERAND1", "text", ""}, {Sb2ArgSpec::Kind::Input, "OPERAND2", "text", ""}}}},
    {">", {"operator_gt", {{Sb2ArgSpec::Kind::Input, "OPERAND1", "text", ""}, {Sb2ArgSpec::Kind::Input, "OPERAND2", "text", ""}}}},
    {"&", {"operator_and", {{Sb2ArgSpec::Kind::Input, "OPERAND1", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "OPERAND2", "boolean", ""}}}},
    {"|", {"operator_or", {{Sb2ArgSpec::Kind::Input, "OPERAND1", "boolean", ""}, {Sb2ArgSpec::Kind::Input, "OPERAND2", "boolean", ""}}}},
    {"not", {"operator_not", {{Sb2ArgSpec::Kind::Input, "OPERAND", "boolean", ""}}}},
    {"concatenate:with:", {"operator_join", {{Sb2ArgSpec::Kind::Input, "STRING1", "text", ""}, {Sb2ArgSpec::Kind::Input, "STRING2", "text", ""}}}},
    {"letter:of:", {"operator_letter_of", {{Sb2ArgSpec::Kind::Input, "LETTER", "math_whole_number", ""}, {Sb2ArgSpec::Kind::Input, "STRING", "text", ""}}}},
    {"stringLength:", {"operator_length", {{Sb2ArgSpec::Kind::Input, "STRING", "text", ""}}}},
    {"%", {"operator_mod", {{Sb2ArgSpec::Kind::Input, "NUM1", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "NUM2", "math_number", ""}}}},
    {"rounded", {"operator_round", {{Sb2ArgSpec::Kind::Input, "NUM", "math_number", ""}}}},
    {"computeFunction:of:", {"operator_mathop", {{Sb2ArgSpec::Kind::Field, "OPERATOR", "", ""}, {Sb2ArgSpec::Kind::Input, "NUM", "math_number", ""}}}},
    {"readVariable", {"data_variable", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}}}},
    {"getVar:", {"data_variable", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}}}},
    {"setVar:to:", {"data_setvariableto", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}, {Sb2ArgSpec::Kind::Input, "VALUE", "text", ""}}}},
    {"changeVar:by:", {"data_changevariableby", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}, {Sb2ArgSpec::Kind::Input, "VALUE", "math_number", ""}}}},
    {"showVariable:", {"data_showvariable", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}}}},
    {"hideVariable:", {"data_hidevariable", {{Sb2ArgSpec::Kind::Field, "VARIABLE", "", ""}}}},
    {"contentsOfList:", {"data_listcontents", {{Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"append:toList:", {"data_addtolist", {{Sb2ArgSpec::Kind::Input, "ITEM", "text", ""}, {Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"deleteLine:ofList:", {"data_deleteoflist", {{Sb2ArgSpec::Kind::Input, "INDEX", "math_integer", ""}, {Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"insert:at:ofList:", {"data_insertatlist", {{Sb2ArgSpec::Kind::Input, "ITEM", "text", ""}, {Sb2ArgSpec::Kind::Input, "INDEX", "math_integer", ""}, {Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"setLine:ofList:to:", {"data_replaceitemoflist", {{Sb2ArgSpec::Kind::Input, "INDEX", "math_integer", ""}, {Sb2ArgSpec::Kind::Field, "LIST", "", "list"}, {Sb2ArgSpec::Kind::Input, "ITEM", "text", ""}}}},
    {"getLine:ofList:", {"data_itemoflist", {{Sb2ArgSpec::Kind::Input, "INDEX", "math_integer", ""}, {Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"lineCountOfList:", {"data_lengthoflist", {{Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"list:contains:", {"data_listcontainsitem", {{Sb2ArgSpec::Kind::Field, "LIST", "", "list"}, {Sb2ArgSpec::Kind::Input, "ITEM", "text", ""}}}},
    {"showList:", {"data_showlist", {{Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"hideList:", {"data_hidelist", {{Sb2ArgSpec::Kind::Field, "LIST", "", "list"}}}},
    {"procDef", {"procedures_definition", {}}},
    {"getParam", {"argument_reporter_string_number", {{Sb2ArgSpec::Kind::Field, "VALUE", "", ""}}}},
    {"call", {"procedures_call", {}}},
    {"LEGO WeDo 2.0motorOnFor", {"wedo2_motorOnFor", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "DURATION", "math_number", ""}}}},
    {"LEGO WeDo 2.0.motorOnFor", {"wedo2_motorOnFor", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "DURATION", "math_number", ""}}}},
    {"LEGO WeDo 2.0motorOn", {"wedo2_motorOn", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}}}},
    {"LEGO WeDo 2.0.motorOn", {"wedo2_motorOn", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}}}},
    {"LEGO WeDo 2.0motorOff", {"wedo2_motorOff", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}}}},
    {"LEGO WeDo 2.0.motorOff", {"wedo2_motorOff", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}}}},
    {"LEGO WeDo 2.0startMotorPower", {"wedo2_startMotorPower", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "POWER", "math_number", ""}}}},
    {"LEGO WeDo 2.0.startMotorPower", {"wedo2_startMotorPower", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "POWER", "math_number", ""}}}},
    {"LEGO WeDo 2.0setMotorDirection", {"wedo2_setMotorDirection", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "MOTOR_DIRECTION", "wedo2_menu_MOTOR_DIRECTION", ""}}}},
    {"LEGO WeDo 2.0.setMotorDirection", {"wedo2_setMotorDirection", {{Sb2ArgSpec::Kind::Input, "MOTOR_ID", "wedo2_menu_MOTOR_ID", ""}, {Sb2ArgSpec::Kind::Input, "MOTOR_DIRECTION", "wedo2_menu_MOTOR_DIRECTION", ""}}}},
    {"LEGO WeDo 2.0setLED", {"wedo2_setLightHue", {{Sb2ArgSpec::Kind::Input, "HUE", "math_number", ""}}}},
    {"LEGO WeDo 2.0.setLED", {"wedo2_setLightHue", {{Sb2ArgSpec::Kind::Input, "HUE", "math_number", ""}}}},
    {"LEGO WeDo 2.0playNote", {"wedo2_playNoteFor", {{Sb2ArgSpec::Kind::Input, "NOTE", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "DURATION", "math_number", ""}}}},
    {"LEGO WeDo 2.0.playNote", {"wedo2_playNoteFor", {{Sb2ArgSpec::Kind::Input, "NOTE", "math_number", ""}, {Sb2ArgSpec::Kind::Input, "DURATION", "math_number", ""}}}},
    {"LEGO WeDo 2.0whenDistance", {"wedo2_whenDistance", {{Sb2ArgSpec::Kind::Input, "OP", "wedo2_menu_OP", ""}, {Sb2ArgSpec::Kind::Input, "REFERENCE", "math_number", ""}}}},
    {"LEGO WeDo 2.0.whenDistance", {"wedo2_whenDistance", {{Sb2ArgSpec::Kind::Input, "OP", "wedo2_menu_OP", ""}, {Sb2ArgSpec::Kind::Input, "REFERENCE", "math_number", ""}}}},
    {"LEGO WeDo 2.0whenTilted", {"wedo2_whenTilted", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION_ANY", "wedo2_menu_TILT_DIRECTION_ANY", ""}}}},
    {"LEGO WeDo 2.0.whenTilted", {"wedo2_whenTilted", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION_ANY", "wedo2_menu_TILT_DIRECTION_ANY", ""}}}},
    {"LEGO WeDo 2.0getDistance", {"wedo2_getDistance", {}}},
    {"LEGO WeDo 2.0.getDistance", {"wedo2_getDistance", {}}},
    {"LEGO WeDo 2.0isTilted", {"wedo2_isTilted", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION_ANY", "wedo2_menu_TILT_DIRECTION_ANY", ""}}}},
    {"LEGO WeDo 2.0.isTilted", {"wedo2_isTilted", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION_ANY", "wedo2_menu_TILT_DIRECTION_ANY", ""}}}},
    {"LEGO WeDo 2.0getTilt", {"wedo2_getTiltAngle", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION", "wedo2_menu_TILT_DIRECTION", ""}}}},
    {"LEGO WeDo 2.0.getTilt", {"wedo2_getTiltAngle", {{Sb2ArgSpec::Kind::Input, "TILT_DIRECTION", "wedo2_menu_TILT_DIRECTION", ""}}}},
};

const std::unordered_map<std::string, Sb2BlockSpec> &getSb2SpecMap() {
    static const std::unordered_map<std::string, Sb2BlockSpec> map = [] {
        std::unordered_map<std::string, Sb2BlockSpec> m;
        m.reserve(std::size(sb2RawSpecs));
        for (const RawEntry &entry : sb2RawSpecs) {
            Sb2BlockSpec spec;
            spec.opcode = entry.spec.opcode;
            for (const RawArg &arg : entry.spec.args) {
                if (arg.name == nullptr) break;
                spec.argMap.push_back({arg.kind, arg.name, arg.inputOp, arg.variableType});
            }
            m.emplace(entry.key, std::move(spec));
        }
        return m;
    }();
    return map;
}
} // namespace

const Sb2BlockSpec *lookupSb2Spec(const std::string &oldOpcode) {
    const auto &specMap = getSb2SpecMap();
    const auto it = specMap.find(oldOpcode);
    if (it == specMap.end()) return nullptr;
    return &it->second;
}
