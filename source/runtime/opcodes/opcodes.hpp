#pragma once

#include <cstdint>

enum class HatType : uint16_t {
    FLAG_CLICKED,
    THIS_SPRITE_CLICKED,
    STAGE_CLICKED,
    BROADCAST_RECEIVED,
    KEY_PRESSED,
    BACKDROP_SWITCHED,
    GREATER_THAN,
    CLONE_START,
};

enum Opcode : uint16_t {
    PUSH_POS_INT,
    PUSH_NEG_INT,
    PUSH_FALSE,
    PUSH_TRUE,
    PUSH_CONST,

    motion_movesteps,
    event_whenflagclicked,
    event_if_else,
    RETURN,

    PUSH_CALL_STACK,

    PUSH_PUB_VAR,
    PUSH_PRI_VAR,
    PUSH_PUB_LIST,
    PUSH_PRI_LIST,

    RETURN_WITH_VALUE,

    JUMP_FWD,
    JUMP_BACK,
    JUMP_FWD_IF_FALSE,
    JUMP_ABS_32,

    CALL_PROCEDURE,
    CALL_PROCEDURE_WOS, // without screen refresh

    event_whenspritesclicked,

    motion_setx,
    motion_sety,
    motion_changexby,
    motion_changeyby,

    looks_costumenumbername_number,
    looks_costumenumbername_name,

    sound_playuntildone,
    sound_play,
    sound_stopallsounds,
    sound_setvolumeto,
    sound_changevolumeby,
    sound_volume,

    PUSH_PROC_ARG,

    MAX_BLOCKS,
};