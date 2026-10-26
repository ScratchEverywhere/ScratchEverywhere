#include "renderer_nv2a.hpp"
#include "image_nv2a.hpp"
#include "speech_manager_nv2a.hpp"
#include "swizzle.h"
#include "xgu.h"
#include "xgux.h"

#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <string>
#include <text.hpp>
#include <types.hpp>
#include <vector>

#ifdef __XBOX__
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/video.h>

#undef min
#undef max

#ifndef VENC_MODE_WIDESCREEN
#define VENC_MODE_WIDESCREEN VIDEO_WIDESCREEN
#endif
#ifndef VENC_MODE_720P
#define VENC_MODE_720P VIDEO_MODE_720P
#endif
#ifndef VENC_MODE_1080I
#define VENC_MODE_1080I VIDEO_MODE_1080I
#endif
#ifndef VENC_MODE_480P
#define VENC_MODE_480P VIDEO_MODE_480P
#endif
#endif

// Ring buffer arena size for direct vertex streaming (3 partitions for triple-buffering)
static constexpr size_t ARENA_PER_FRAME_SIZE = 512 * 1024;
static constexpr size_t VERTEX_ARENA_SIZE = ARENA_PER_FRAME_SIZE * 3;
static constexpr size_t PEN_FLUSH_THRESHOLD = 4096;

static uint8_t *s_vertexArena = nullptr;
static uint32_t s_vertexArenaPhys = 0;
static size_t s_arenaBase = 0;
static size_t s_arenaOffset = 0;
static uint32_t s_frameIndex = 0;

extern "C" unsigned int pb_ColorFmt;

static inline uint32_t npot2pot(uint32_t num) {
    if (num <= 1) return 1;
    return 1 << (32 - __builtin_clz(num - 1));
}

static Renderer_NV2A::VideoDisplaySettings s_nv2aVideoSettings = {
    640, 480, 32, 60, false, false, false, false, 0.04f, 0
};
static bool s_videoInitialized = false;

static uint32_t mapSmcToAvPack(uint32_t smcVal) {
    switch (smcVal & 0x07) {
        case 0x00: return AV_PACK_SCART;
        case 0x01: return AV_PACK_HDTV;
        case 0x02: return AV_PACK_VGA;
        case 0x03: return AV_PACK_RFU;
        case 0x04: return AV_PACK_SVIDEO;
        case 0x06: return AV_PACK_STANDARD;
        case 0x07: return AV_PACK_NONE;
        default:   return AV_PACK_NONE;
    }
}

void Renderer_NV2A::initVideo() {
    if (s_videoInitialized) return;
    s_videoInitialized = true;

#ifdef __XBOX__
    ULONG encoderSettings = XVideoGetEncoderSettings();
    bool isWidescreen = (encoderSettings & VENC_MODE_WIDESCREEN) != 0;
    bool supports720p = (encoderSettings & VENC_MODE_720P) != 0;
    bool supports480p = (encoderSettings & VENC_MODE_480P) != 0;
    bool supports1080i = (encoderSettings & VENC_MODE_1080I) != 0;

    s_nv2aVideoSettings.isWidescreen = isWidescreen;
    s_nv2aVideoSettings.supports720p = supports720p;
    s_nv2aVideoSettings.supports480p = supports480p;
    s_nv2aVideoSettings.supports1080i = supports1080i;

    ULONG smcAvPack = 0;
    NTSTATUS smbStatus = HalReadSMBusValue(0x20, 0x04, FALSE, &smcAvPack);
    uint32_t resolvedPack = AV_PACK_NONE;
    if (NT_SUCCESS(smbStatus)) {
        resolvedPack = mapSmcToAvPack(smcAvPack);
    }
    if (resolvedPack == AV_PACK_NONE) {
        resolvedPack = static_cast<uint32_t>(encoderSettings & VIDEO_ADAPTER_MASK);
    }
    s_nv2aVideoSettings.avPackType = resolvedPack;

    bool can720p = false;
    if (supports720p && (s_nv2aVideoSettings.avPackType == AV_PACK_HDTV || s_nv2aVideoSettings.avPackType == AV_PACK_VGA)) {
        VIDEO_MODE testMode;
        void *p = NULL;
        while (XVideoListModes(&testMode, 32, 60, &p)) {
            if (testMode.width == 1280 && testMode.height == 720) {
                can720p = true;
                break;
            }
        }
    }

    s_nv2aVideoSettings.width = can720p ? 1280 : 640;
    s_nv2aVideoSettings.height = can720p ? 720 : 480;
    s_nv2aVideoSettings.bpp = 32;
    s_nv2aVideoSettings.refresh = 60;

    float autoOverscan = 0.0f;
    switch (s_nv2aVideoSettings.avPackType) {
        case AV_PACK_VGA:
            autoOverscan = 0.0f;
            break;
        case AV_PACK_HDTV:
            if (s_nv2aVideoSettings.width == 640 && s_nv2aVideoSettings.height == 480 && !s_nv2aVideoSettings.supports480p) {
                autoOverscan = 0.04f;
            } else {
                autoOverscan = 0.0f;
            }
            break;
        default:
            if (s_nv2aVideoSettings.width == 640 && s_nv2aVideoSettings.height == 480) {
                autoOverscan = 0.04f;
            } else {
                autoOverscan = 0.0f;
            }
            break;
    }
    s_nv2aVideoSettings.overscanRatio = autoOverscan;

    char vidLog[256];
    snprintf(vidLog, sizeof(vidLog),
             "NV2A Video: %s cable detected. Mode %dx%d@%dHz (widescreen: %s, 720p: %s, overscan: %d%%)",
             getAVPackName(), s_nv2aVideoSettings.width, s_nv2aVideoSettings.height, s_nv2aVideoSettings.refresh,
             s_nv2aVideoSettings.isWidescreen ? "yes" : "no",
             s_nv2aVideoSettings.supports720p ? "yes" : "no",
             static_cast<int>(s_nv2aVideoSettings.overscanRatio * 100.0f));
    Log::log(vidLog);
#endif
}

Renderer_NV2A::VideoDisplaySettings Renderer_NV2A::getVideoDisplaySettings() {
    initVideo();
    return s_nv2aVideoSettings;
}

uint32_t Renderer_NV2A::getAVPackType() {
    initVideo();
    return s_nv2aVideoSettings.avPackType;
}

const char *Renderer_NV2A::getAVPackName() {
    initVideo();
    switch (s_nv2aVideoSettings.avPackType) {
        case AV_PACK_STANDARD: return "Composite (Standard)";
        case AV_PACK_RFU:      return "RF Adapter";
        case AV_PACK_SCART:    return "SCART (RGB)";
        case AV_PACK_HDTV:     return "HDTV (Component)";
        case AV_PACK_VGA:      return "VGA";
        case AV_PACK_SVIDEO:   return "S-Video";
        default:               return "None / Unknown";
    }
}

static float s_mainViewportScaleX = 1.0f;
static float s_mainViewportScaleY = 1.0f;
static float s_mainViewportOffsetX = 0.0f;
static float s_mainViewportOffsetY = 0.0f;
static int s_stageScissorX = 0;
static int s_stageScissorY = 0;
static int s_stageScissorW = 640;
static int s_stageScissorH = 480;

static void updateViewportOverscan();

void Renderer_NV2A::applyVideoSettings() {
    applyOverscan(s_nv2aVideoSettings.overscanRatio);
}

void Renderer_NV2A::applyOverscan(float overscanRatio) {
    s_nv2aVideoSettings.overscanRatio = overscanRatio;
    updateViewportOverscan();
}

void Renderer_NV2A::applyAspectRatio(int logicalWidth, int logicalHeight, bool isWidescreen) {
    (void)logicalWidth;
    (void)logicalHeight;
    s_nv2aVideoSettings.isWidescreen = isWidescreen;
}

void Renderer_NV2A::setOverscanRatio(float ratio) {
    s_nv2aVideoSettings.overscanRatio = ratio;
    updateViewportOverscan();
}

float Renderer_NV2A::getOverscanRatio() {
    return s_nv2aVideoSettings.overscanRatio;
}

bool Renderer_NV2A::isWidescreen() {
    initVideo();
    return s_nv2aVideoSettings.isWidescreen;
}

bool Renderer_NV2A::supports720p() {
    initVideo();
    return s_nv2aVideoSettings.supports720p;
}

bool Renderer_NV2A::supports480p() {
    initVideo();
    return s_nv2aVideoSettings.supports480p;
}

bool Renderer_NV2A::supports1080i() {
    initVideo();
    return s_nv2aVideoSettings.supports1080i;
}

void Renderer_NV2A::getVideoMode(int &width, int &height, int &bpp, int &refresh) {
    initVideo();
#ifdef __XBOX__
    VIDEO_MODE currentMode = XVideoGetMode();
    width = currentMode.width > 0 ? currentMode.width : s_nv2aVideoSettings.width;
    height = currentMode.height > 0 ? currentMode.height : s_nv2aVideoSettings.height;
    bpp = currentMode.bpp > 0 ? currentMode.bpp : s_nv2aVideoSettings.bpp;
    refresh = currentMode.refresh > 0 ? currentMode.refresh : s_nv2aVideoSettings.refresh;
#else
    width = s_nv2aVideoSettings.width;
    height = s_nv2aVideoSettings.height;
    bpp = s_nv2aVideoSettings.bpp;
    refresh = s_nv2aVideoSettings.refresh;
#endif
}

// Combiner helpers for NV2A registers
static inline void combinerInit() {
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_SHADER_OTHER_STAGE_INPUT,
        XGU_MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE1, 0) |
        XGU_MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE2, 0) |
        XGU_MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE3, 0));
    p = pb_push1(p, NV097_SET_SHADER_STAGE_PROGRAM,
        XGU_MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE0, NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_PROGRAM_NONE) |
        XGU_MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE1, NV097_SET_SHADER_STAGE_PROGRAM_STAGE1_PROGRAM_NONE) |
        XGU_MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE2, NV097_SET_SHADER_STAGE_PROGRAM_STAGE2_PROGRAM_NONE) |
        XGU_MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE3, NV097_SET_SHADER_STAGE_PROGRAM_STAGE3_PROGRAM_NONE));

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, 0x1) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_MAP, 0x1) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, 1));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW0,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_INVERSE, 0));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW1,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_SPECULAR_CLAMP, 0));
    pb_end(p);
}

static inline void unlitCombinerApply() {
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_SHADER_OTHER_STAGE_INPUT, 0);
    p = pb_push1(p, NV097_SET_SHADER_STAGE_PROGRAM, 0);

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, 0x1) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_MAP, 0x1) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, 1));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW0,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_INVERSE, 0));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW1,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_SPECULAR_CLAMP, 0));
    pb_end(p);
}

static inline void textureCombinerApply() {
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_SHADER_OTHER_STAGE_INPUT, 0);
    p = pb_push1(p, NV097_SET_SHADER_STAGE_PROGRAM, XGU_MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE0, NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_2D_PROJECTIVE));

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, 0x8) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_COLOR_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_SOURCE, 0x8) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_A_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_B_MAP, 0x6) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_C_MAP, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_ALPHA_ICW_D_MAP, 0x0));

    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_OCW + 0 * 4,
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_AB_DST, 0x4) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_CD_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_SUM_DST, 0x0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_MUX_ENABLE, 0) |
        XGU_MASK(NV097_SET_COMBINER_ALPHA_OCW_OP, 0));

    p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_SAME_FACTOR_ALL) |
        XGU_MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, 1));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW0,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_INVERSE, 0));

    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW1,
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE, 0x0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_ALPHA, 0) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE, 0x4) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA, 1) | XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_INVERSE, 0) |
        XGU_MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_SPECULAR_CLAMP, 0));
    pb_end(p);
}

// Pen & RenderTarget State
static NV2ATexture s_penTexture;
static bool s_hasPenTexture = false;
static s_CtxDma s_penDmaCtx;
static bool s_penTargetActive = false;
static float s_penScaleY = 1.0f;
static std::vector<NV2AVertex> s_penVerts;
static bool s_lastCapValid = false;
static float s_lastCapX = 0.0f, s_lastCapY = 0.0f, s_lastCapRadius = 0.0f;
static uint32_t s_lastCapColor = 0;

static void updateViewportOverscan() {
    float ratio = s_nv2aVideoSettings.overscanRatio;
    int width = 0, height = 0;
#ifdef __XBOX__
    width = pb_back_buffer_width();
    height = pb_back_buffer_height();
#endif
    if (width <= 0) width = s_nv2aVideoSettings.width;
    if (height <= 0) height = s_nv2aVideoSettings.height;

    if (ratio <= 0.0f) {
        s_mainViewportScaleX = 1.0f;
        s_mainViewportScaleY = 1.0f;
        s_mainViewportOffsetX = 0.0f;
        s_mainViewportOffsetY = 0.0f;
    } else {
        float totalW = static_cast<float>(width);
        float totalH = static_cast<float>(height);
        float offsetX = std::floor(totalW * ratio);
        float offsetY = std::floor(totalH * ratio);
        float safeW = totalW - (offsetX * 2.0f);
        float safeH = totalH - (offsetY * 2.0f);
        s_mainViewportScaleX = safeW / totalW;
        s_mainViewportScaleY = safeH / totalH;
        s_mainViewportOffsetX = offsetX;
        s_mainViewportOffsetY = offsetY;
    }

    float screenW = static_cast<float>(width);
    float screenH = static_cast<float>(height);
    float screenAspect = screenW / screenH;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float stageX = 0, stageY = 0, stageW = screenW, stageH = screenH;
    if (screenAspect > projectAspect) {
        float scale = screenH / Scratch::projectHeight;
        stageW = Scratch::projectWidth * scale;
        stageX = (screenW - stageW) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scale = screenW / Scratch::projectWidth;
        stageH = Scratch::projectHeight * scale;
        stageY = (screenH - stageH) / 2.0f;
    }

    s_stageScissorX = static_cast<int>(std::round(stageX * s_mainViewportScaleX + s_mainViewportOffsetX));
    s_stageScissorY = static_cast<int>(std::round(stageY * s_mainViewportScaleY + s_mainViewportOffsetY));
    s_stageScissorW = static_cast<int>(std::round(stageW * s_mainViewportScaleX));
    s_stageScissorH = static_cast<int>(std::round(stageH * s_mainViewportScaleY));
}

// Circle calculation table
struct CirclePoint {
    float c, s;
};

static const CirclePoint *getCircleTable(unsigned int segments) {
    static std::vector<std::vector<CirclePoint>> tables;
    if (segments >= tables.size()) tables.resize(segments + 1);
    std::vector<CirclePoint> &table = tables[segments];
    if (table.empty()) {
        table.resize(segments + 1);
        const double step = 2.0 * M_PI / segments;
        for (unsigned int i = 0; i <= segments; ++i) {
            table[i] = {static_cast<float>(std::cos(i * step)), static_cast<float>(std::sin(i * step))};
        }
    }
    return table.data();
}

static unsigned int circleSegmentsFor(double base, double penSize) {
    const double wanted = std::max(8.0, base * (penSize / 150.0));
    return std::min(static_cast<unsigned int>(wanted), 128u);
}

Renderer_NV2A::Renderer_NV2A() {
    initVideo();
    int windowWidth = s_nv2aVideoSettings.width;
    int windowHeight = s_nv2aVideoSettings.height;

    window = createWindow(windowWidth, windowHeight, "Scratch Everywhere!", WindowingBackends::NV2A);
    if (!window || window->error.has_value()) {
        error = "Failed to create NV2A window";
        return;
    }

#ifdef __XBOX__
    pb_size(512 * 1024);
    pb_set_color_format(NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8, false);

    int pb_res = pb_init();
    if (pb_res < 0 && pb_res != -8) {
        error = "pb_init failed: " + std::to_string(pb_res);
        return;
    }

    XVideoSetVideoEnable(true);
    pb_show_front_screen();
    pb_target_back_buffer();

    // Allocate vertex ring buffer in write-combined memory (6MB triple-buffered)
    s_vertexArena = static_cast<uint8_t *>(MmAllocateContiguousMemoryEx(VERTEX_ARENA_SIZE, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE));
    if (s_vertexArena) {
        s_vertexArenaPhys = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(MmGetPhysicalAddress(s_vertexArena)));
    } else {
        s_vertexArenaPhys = 0;
    }
    s_arenaBase = 0;
    s_arenaOffset = 0;
    s_frameIndex = 0;

    const float m_identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    uint32_t *p = pb_begin();
    p = xgu_set_blend_enable(p, true);
    p = xgu_set_depth_test_enable(p, false);
    p = xgu_set_dither_enable(p, false);
    p = xgu_set_blend_func_sfactor(p, XGU_FACTOR_SRC_ALPHA);
    p = xgu_set_blend_func_dfactor(p, XGU_FACTOR_ONE_MINUS_SRC_ALPHA);
    p = xgu_set_skin_mode(p, XGU_SKIN_MODE_OFF);
    p = xgu_set_normalization_enable(p, false);
    p = xgu_set_lighting_enable(p, false);
    p = xgu_set_cull_face_enable(p, false);
    p = xgu_set_transform_execution_mode(p, XGU_FIXED, XGU_RANGE_MODE_PRIVATE);
    p = xgu_set_projection_matrix(p, m_identity);
    p = xgu_set_composite_matrix(p, m_identity);
    p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
    p = xgu_set_viewport_scale(p, 1.0f, 1.0f, 1.0f, 1.0f);
    p = xgu_set_scissor_rect(p, false, 0, 0, pb_back_buffer_width(), pb_back_buffer_height());
    pb_end(p);

    for (int i = 0; i < 4; i++) {
        p = pb_begin();
        p = xgu_set_texgen_s(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_t(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_r(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_q(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texture_matrix_enable(p, i, false);
        p = xgu_set_texture_matrix(p, i, m_identity);
        pb_end(p);
    }

    for (int i = 0; i < 4; i++) {
        p = pb_begin();
        p = xgu_set_model_view_matrix(p, i, m_identity);
        p = xgu_set_inverse_model_view_matrix(p, i, m_identity);
        pb_end(p);
    }

    p = pb_begin();
    p = push_command(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT, 16);
    for (int i = 0; i < 16; i++) {
        p = push_parameter(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    }
    pb_end(p);

    combinerInit();
    unlitCombinerApply();

    // Create DMA channel for off-screen pen rendering (channel 3)
    pb_create_dma_ctx(3, DMA_CLASS_3D, 0, MAXRAM, &s_penDmaCtx);
    pb_bind_channel(&s_penDmaCtx);

    applyVideoSettings();

    Log::log("Renderer_NV2A hardware initialized: " + std::to_string(windowWidth) + "x" + std::to_string(windowHeight));
#endif
}

Renderer_NV2A::~Renderer_NV2A() {
    cleanupPen();
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    TextObject::cleanupText();

#ifdef __XBOX__
    if (s_vertexArena) {
        MmFreeContiguousMemory(s_vertexArena);
        s_vertexArena = nullptr;
        s_vertexArenaPhys = 0;
    }
    pb_kill();
#endif

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }
}

NV2ATexture *Renderer_NV2A::createTexture(int width, int height, uint32_t format, bool swizzled) {
    auto *tex = new NV2ATexture();
    tex->texWidth = width;
    tex->texHeight = height;
    tex->format = format;
    tex->swizzled = swizzled;

    tex->isDXT = (format == XGU_TEXTURE_FORMAT_DXT1 || format == XGU_TEXTURE_FORMAT_DXT5);

    if (tex->isDXT) {
        tex->swizzled = false;
        tex->dataWidth = npot2pot(width);
        tex->dataHeight = npot2pot(height);
        tex->uScale = static_cast<float>(width) / static_cast<float>(tex->dataWidth);
        tex->vScale = static_cast<float>(height) / static_cast<float>(tex->dataHeight);
        int blockBytes = (format == XGU_TEXTURE_FORMAT_DXT5) ? 16 : 8;
        tex->pitch = std::max(64, ((tex->dataWidth + 3) / 4) * blockBytes);
        tex->bytesPerPixel = 1;
        tex->allocationSize = ((tex->dataHeight + 3) / 4) * tex->pitch;
    } else {
        if (format == XGU_TEXTURE_FORMAT_R5G6B5_SWIZZLED || format == XGU_TEXTURE_FORMAT_A1R5G5B5_SWIZZLED || format == XGU_TEXTURE_FORMAT_A4R4G4B4_SWIZZLED) {
            tex->bytesPerPixel = 2;
        } else {
            tex->bytesPerPixel = 4;
        }

        if (swizzled) {
            tex->dataWidth = npot2pot(width);
            tex->dataHeight = npot2pot(height);
            tex->uScale = static_cast<float>(width) / static_cast<float>(tex->dataWidth);
            tex->vScale = static_cast<float>(height) / static_cast<float>(tex->dataHeight);
        } else {
            tex->dataWidth = (width + (64 / tex->bytesPerPixel) - 1) & ~((64 / tex->bytesPerPixel) - 1);
            tex->dataHeight = height;
            tex->uScale = static_cast<float>(width);
            tex->vScale = static_cast<float>(height);
        }
        tex->pitch = tex->dataWidth * tex->bytesPerPixel;
        tex->allocationSize = tex->dataHeight * tex->pitch;
    }

#ifdef __XBOX__
    tex->data = static_cast<uint8_t *>(MmAllocateContiguousMemoryEx(tex->allocationSize, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE));
    if (!tex->data) {
        Scratch::freeUnusedCostumeImages();
        tex->data = static_cast<uint8_t *>(MmAllocateContiguousMemoryEx(tex->allocationSize, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE));
    }
    if (!tex->data) {
        delete tex;
        return nullptr;
    }
    tex->dataPhysicalAddress = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(MmGetPhysicalAddress(tex->data)));
    memset(tex->data, 0, tex->allocationSize);
#endif
    return tex;
}

static const NV2ATexture *s_activeTexture = nullptr;
static uint32_t s_frameCounter = 1;

uint32_t Renderer_NV2A::getFrameCounter() {
    return s_frameCounter;
}
static bool s_activeLinear = false;
static NV2ABlendMode s_activeBlendMode = NV2ABlendMode::None;
static int s_activeShader = -1; // 0 = unlit, 1 = texture

void Renderer_NV2A::destroyTexture(NV2ATexture *&texture) {
    if (!texture) return;
    if (s_activeTexture == texture) {
        s_activeTexture = nullptr;
    }
#ifdef __XBOX__
    while (pb_busy()) {
        Sleep(0);
    }
    if (texture->data) {
        MmFreeContiguousMemory(texture->data);
        texture->data = nullptr;
    }
#endif
    delete texture;
    texture = nullptr;
}

bool Renderer_NV2A::updateTexture(NV2ATexture *texture, const void *pixels, int pitch) {
    if (!texture || !texture->data || !pixels) return false;

    if (texture->isDXT) {
        int blockBytes = (texture->format == XGU_TEXTURE_FORMAT_DXT5) ? 16 : 8;
        int rowBytes = ((texture->dataWidth + 3) / 4) * blockBytes;
        int blockRows = (texture->dataHeight + 3) / 4;
        for (int r = 0; r < blockRows; ++r) {
            memcpy(texture->data + static_cast<size_t>(r) * texture->pitch,
                   static_cast<const uint8_t *>(pixels) + static_cast<size_t>(r) * pitch, rowBytes);
        }
    } else if (texture->swizzled) {
        swizzle_rect(static_cast<const uint8_t *>(pixels), texture->texWidth, texture->texHeight,
                     texture->data, pitch, texture->bytesPerPixel);
    } else {
        for (int r = 0; r < texture->texHeight; ++r) {
            memcpy(texture->data + static_cast<size_t>(r) * texture->pitch,
                   static_cast<const uint8_t *>(pixels) + static_cast<size_t>(r) * pitch,
                   texture->texWidth * texture->bytesPerPixel);
        }
    }

#ifdef __XBOX__
    __asm__ __volatile__ ("sfence" ::: "memory");
#endif
    return true;
}

void Renderer_NV2A::bindTexture(const NV2ATexture *texture, bool linear) {
    if (!texture || !texture->data) return;

    if (s_activeShader != 1) {
        textureCombinerApply();
        s_activeShader = 1;
    }

    if (s_activeTexture != texture || s_activeLinear != linear) {
        uint32_t *p = pb_begin();
        p = xgu_set_texture_offset(p, 0, texture->dataPhysicalAddress);
        p = xgu_set_texture_format(p, 0, 2, false, XGU_SOURCE_COLOR, 2,
                                   static_cast<XguTexFormatColor>(texture->format), 1,
                                   static_cast<uint8_t>(__builtin_ctz(npot2pot(texture->dataWidth))),
                                   static_cast<uint8_t>(__builtin_ctz(npot2pot(texture->dataHeight))), 0);
        p = xgu_set_texture_control0(p, 0, true, 0, 0);
        p = xgu_set_texture_control1(p, 0, std::max(64, texture->pitch));
        p = xgu_set_texture_image_rect(p, 0, texture->texWidth, texture->texHeight);

        XguTexFilter filter = linear ? XGU_TEXTURE_FILTER_LINEAR : XGU_TEXTURE_FILTER_NEAREST;
        p = xgu_set_texture_filter(p, 0, 0, XGU_TEXTURE_CONVOLUTION_GAUSSIAN,
                                   filter, filter, false, false, false, false);
        p = xgu_set_texture_address(p, 0, XGU_CLAMP_TO_EDGE, false, XGU_CLAMP_TO_EDGE, false, XGU_CLAMP_TO_EDGE, false, false);
        pb_end(p);
        s_activeTexture = texture;
        s_activeLinear = linear;
    }
}

void Renderer_NV2A::unbindTexture() {
    if (s_activeShader != 0) {
        unlitCombinerApply();
        s_activeShader = 0;
    }
    s_activeTexture = nullptr;
}

void Renderer_NV2A::setBlendMode(NV2ABlendMode mode) {
    if (s_activeBlendMode == mode) return;
    s_activeBlendMode = mode;

    XguBlendFactor sfactor = XGU_FACTOR_SRC_ALPHA;
    XguBlendFactor dfactor = XGU_FACTOR_ONE_MINUS_SRC_ALPHA;

    switch (mode) {
        case NV2ABlendMode::None:
            sfactor = XGU_FACTOR_ONE;
            dfactor = XGU_FACTOR_ZERO;
            break;
        case NV2ABlendMode::Blend:
            sfactor = XGU_FACTOR_SRC_ALPHA;
            dfactor = XGU_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case NV2ABlendMode::Add:
            sfactor = XGU_FACTOR_SRC_ALPHA;
            dfactor = XGU_FACTOR_ONE;
            break;
    }

    uint32_t *p = pb_begin();
    p = xgu_set_blend_func_sfactor(p, sfactor);
    p = xgu_set_blend_func_dfactor(p, dfactor);
    pb_end(p);
}

void Renderer_NV2A::setViewport(int x, int y, int w, int h) {
    uint32_t *p = pb_begin();
    p = xgu_set_viewport_offset(p, static_cast<float>(x), static_cast<float>(y), 0.0f, 0.0f);
    pb_end(p);
}

void Renderer_NV2A::setScissor(int x, int y, int w, int h) {
    uint32_t *p = pb_begin();
    if (s_penTargetActive) {
        p = xgu_set_scissor_rect(p, false, x, y, w, h);
    } else {
        int sx = static_cast<int>(std::round(x * s_mainViewportScaleX + s_mainViewportOffsetX));
        int sy = static_cast<int>(std::round(y * s_mainViewportScaleY + s_mainViewportOffsetY));
        int sw = static_cast<int>(std::round(w * s_mainViewportScaleX));
        int sh = static_cast<int>(std::round(h * s_mainViewportScaleY));
        int x1 = std::max(sx, s_stageScissorX);
        int y1 = std::max(sy, s_stageScissorY);
        int x2 = std::min(sx + sw, s_stageScissorX + s_stageScissorW);
        int y2 = std::min(sy + sh, s_stageScissorY + s_stageScissorH);
        int fw = std::max(0, x2 - x1);
        int fh = std::max(0, y2 - y1);
        p = xgu_set_scissor_rect(p, false, x1, y1, fw, fh);
    }
    pb_end(p);
}

void Renderer_NV2A::clearScissor() {
    uint32_t *p = pb_begin();
    if (s_penTargetActive) {
        p = xgu_set_scissor_rect(p, false, 0, 0, s_penTexture.texWidth, s_penTexture.texHeight);
    } else {
        p = xgu_set_scissor_rect(p, false, s_stageScissorX, s_stageScissorY, s_stageScissorW, s_stageScissorH);
    }
    pb_end(p);
}

void Renderer_NV2A::drawTriangles(const NV2AVertex *vertices, size_t count) {
    if (!vertices || count == 0 || !s_vertexArena) return;

    size_t byteSize = count * sizeof(NV2AVertex);
    byteSize = (byteSize + 31) & ~31;

    if (s_arenaOffset + byteSize > s_arenaBase + ARENA_PER_FRAME_SIZE) {
        while (pb_busy()) {
            __asm__ __volatile__("pause");
        }
        s_arenaOffset = s_arenaBase;
    }

    const size_t vertOffset = s_arenaOffset;
    auto *dstVerts = reinterpret_cast<NV2AVertex *>(s_vertexArena + vertOffset);
    if (!s_penTargetActive && (s_mainViewportScaleX != 1.0f || s_mainViewportScaleY != 1.0f || s_mainViewportOffsetX != 0.0f || s_mainViewportOffsetY != 0.0f)) {
        for (size_t i = 0; i < count; ++i) {
            dstVerts[i].pos[0] = vertices[i].pos[0] * s_mainViewportScaleX + s_mainViewportOffsetX;
            dstVerts[i].pos[1] = vertices[i].pos[1] * s_mainViewportScaleY + s_mainViewportOffsetY;
            dstVerts[i].color[0] = vertices[i].color[0];
            dstVerts[i].color[1] = vertices[i].color[1];
            dstVerts[i].color[2] = vertices[i].color[2];
            dstVerts[i].color[3] = vertices[i].color[3];
            dstVerts[i].tex[0] = vertices[i].tex[0];
            dstVerts[i].tex[1] = vertices[i].tex[1];
        }
    } else {
        memcpy(dstVerts, vertices, count * sizeof(NV2AVertex));
    }
    s_arenaOffset += byteSize;

#ifdef __XBOX__
    __asm__ __volatile__ ("sfence" ::: "memory");
    const uint32_t curPhys = s_vertexArenaPhys + static_cast<uint32_t>(vertOffset);
#else
    const uint32_t curPhys = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s_vertexArena + vertOffset));
#endif

    const uint32_t posAddr = (curPhys + offsetof(NV2AVertex, pos)) & 0x03FFFFFF;
    const uint32_t colAddr = (curPhys + offsetof(NV2AVertex, color)) & 0x03FFFFFF;
    const uint32_t texAddr = (curPhys + offsetof(NV2AVertex, tex)) & 0x03FFFFFF;

    uint32_t *p = pb_begin();
    p = xgu_set_vertex_data_array_format(p, XGU_VERTEX_ARRAY, XGU_FLOAT, 2, sizeof(NV2AVertex));
    p = xgu_set_vertex_data_array_offset(p, XGU_VERTEX_ARRAY, reinterpret_cast<const void *>(static_cast<uintptr_t>(posAddr)));
    p = xgu_set_vertex_data_array_format(p, XGU_COLOR_ARRAY, XGU_UNSIGNED_BYTE_OGL, 4, sizeof(NV2AVertex));
    p = xgu_set_vertex_data_array_offset(p, XGU_COLOR_ARRAY, reinterpret_cast<const void *>(static_cast<uintptr_t>(colAddr)));
    p = xgu_set_vertex_data_array_format(p, XGU_TEXCOORD0_ARRAY, XGU_FLOAT, 2, sizeof(NV2AVertex));
    p = xgu_set_vertex_data_array_offset(p, XGU_TEXCOORD0_ARRAY, reinterpret_cast<const void *>(static_cast<uintptr_t>(texAddr)));
    pb_end(p);

    xgux_draw_arrays(XGU_TRIANGLES, 0, count);
}

void Renderer_NV2A::drawColorTriangles(const NV2AVertex *vertices, size_t count) {
    unbindTexture();
    drawTriangles(vertices, count);
}

static void ensureMainTarget() {
    if (!s_penTargetActive) return;
    s_penTargetActive = false;

    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_WAIT_FOR_IDLE, 0);
    pb_end(p);

    pb_target_back_buffer();

    p = pb_begin();
    p = pb_push1(p, NV097_SET_CONTEXT_DMA_COLOR, DMA_CHANNEL_PIXEL_RENDERER);
    p = pb_push1(p, NV097_SET_SURFACE_PITCH,
                 XGU_MASK(NV097_SET_SURFACE_PITCH_COLOR, pb_back_buffer_pitch()) |
                 XGU_MASK(NV097_SET_SURFACE_PITCH_ZETA, pb_back_buffer_width() * 4));
    p = pb_push1(p, NV097_SET_SURFACE_COLOR_OFFSET, 0);
    p = pb_push1(p, NV097_SET_SURFACE_CLIP_HORIZONTAL,
                 XGU_MASK(NV097_SET_SURFACE_CLIP_HORIZONTAL_WIDTH, pb_back_buffer_width()) |
                 XGU_MASK(NV097_SET_SURFACE_CLIP_HORIZONTAL_X, 0));
    p = pb_push1(p, NV097_SET_SURFACE_CLIP_VERTICAL,
                 XGU_MASK(NV097_SET_SURFACE_CLIP_VERTICAL_HEIGHT, pb_back_buffer_height()) |
                 XGU_MASK(NV097_SET_SURFACE_CLIP_VERTICAL_Y, 0));
    p = pb_push1(p, NV097_SET_SURFACE_FORMAT,
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_COLOR, pb_ColorFmt) |
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_ZETA, NV097_SET_SURFACE_FORMAT_ZETA_Z24S8) |
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_TYPE, NV097_SET_SURFACE_FORMAT_TYPE_PITCH));
    p = xgu_set_viewport_scale(p, 1.0f, 1.0f, 1.0f, 1.0f);
    p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
    p = xgu_set_scissor_rect(p, false, s_stageScissorX, s_stageScissorY, s_stageScissorW, s_stageScissorH);
    pb_end(p);

    s_activeTexture = nullptr;
    s_activeShader = -1;
}

static void ensurePenTarget() {
    if (s_penTargetActive || !s_hasPenTexture) return;
    s_penTargetActive = true;

    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_CONTEXT_DMA_COLOR, s_penDmaCtx.ChannelID);
    p = pb_push1(p, NV097_SET_SURFACE_PITCH,
                 XGU_MASK(NV097_SET_SURFACE_PITCH_COLOR, s_penTexture.pitch) |
                 XGU_MASK(NV097_SET_SURFACE_PITCH_ZETA, pb_back_buffer_width() * 4));
    p = pb_push1(p, NV097_SET_SURFACE_COLOR_OFFSET, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s_penTexture.dataPhysicalAddress)));
    p = pb_push1(p, NV097_SET_SURFACE_CLIP_HORIZONTAL,
                 XGU_MASK(NV097_SET_SURFACE_CLIP_HORIZONTAL_WIDTH, s_penTexture.texWidth) |
                 XGU_MASK(NV097_SET_SURFACE_CLIP_HORIZONTAL_X, 0));
    p = pb_push1(p, NV097_SET_SURFACE_CLIP_VERTICAL,
                 XGU_MASK(NV097_SET_SURFACE_CLIP_VERTICAL_HEIGHT, s_penTexture.texHeight) |
                 XGU_MASK(NV097_SET_SURFACE_CLIP_VERTICAL_Y, 0));
    p = pb_push1(p, NV097_SET_SURFACE_FORMAT,
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_COLOR, NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8) |
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_ZETA, NV097_SET_SURFACE_FORMAT_ZETA_Z24S8) |
                 XGU_MASK(NV097_SET_SURFACE_FORMAT_TYPE, NV097_SET_SURFACE_FORMAT_TYPE_PITCH));
    p = xgu_set_viewport_scale(p, 1.0f, 1.0f, 1.0f, 1.0f);
    p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
    p = xgu_set_scissor_rect(p, false, 0, 0, s_penTexture.texWidth, s_penTexture.texHeight);
    pb_end(p);

    s_activeTexture = nullptr;
    s_activeShader = -1;
}

static void flushPenVerts(Renderer_NV2A *r) {
    if (s_penVerts.empty() || !s_hasPenTexture) return;
    ensurePenTarget();
    r->unbindTexture();
    r->setBlendMode(NV2ABlendMode::Blend);
    r->drawTriangles(s_penVerts.data(), s_penVerts.size());
    s_penVerts.clear();
}

bool Renderer_NV2A::initPen() {
    if (s_hasPenTexture) return true;

    int penW = Scratch::projectWidth > 0 ? Scratch::projectWidth : 480;
    int penH = Scratch::projectHeight > 0 ? Scratch::projectHeight : 360;
    if (Scratch::hqpen) {
        penW = getWidth();
        penH = getHeight();
    }

    s_penTexture.texWidth = penW;
    s_penTexture.texHeight = penH;
    s_penTexture.bytesPerPixel = 4;
    s_penTexture.pitch = (penW * 4 + 63) & ~63;
    s_penTexture.dataWidth = s_penTexture.pitch / 4;
    s_penTexture.dataHeight = penH;
    s_penTexture.uScale = static_cast<float>(penW);
    s_penTexture.vScale = static_cast<float>(penH);
    s_penTexture.swizzled = false;
    s_penTexture.isDXT = false;
    s_penTexture.format = XGU_TEXTURE_FORMAT_A8R8G8B8;
    s_penTexture.allocationSize = s_penTexture.pitch * penH;

#ifdef __XBOX__
    s_penTexture.data = static_cast<uint8_t *>(MmAllocateContiguousMemoryEx(s_penTexture.allocationSize, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE));
    if (!s_penTexture.data) {
        Scratch::freeUnusedCostumeImages();
        s_penTexture.data = static_cast<uint8_t *>(MmAllocateContiguousMemoryEx(s_penTexture.allocationSize, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE));
    }
    if (!s_penTexture.data) {
        return false;
    }
    s_penTexture.dataPhysicalAddress = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(MmGetPhysicalAddress(s_penTexture.data)));
    memset(s_penTexture.data, 0, s_penTexture.allocationSize);
#endif

    s_hasPenTexture = true;
    s_penScaleY = static_cast<float>(penH) / static_cast<float>(Scratch::projectHeight > 0 ? Scratch::projectHeight : 360);
    s_penVerts.clear();
    s_penVerts.reserve(PEN_FLUSH_THRESHOLD + 1024);
    s_lastCapValid = false;

    return true;
}

void Renderer_NV2A::cleanupPen() {
    if (s_hasPenTexture) {
        ensureMainTarget();
#ifdef __XBOX__
        if (s_penTexture.data) {
            MmFreeContiguousMemory(s_penTexture.data);
            s_penTexture.data = nullptr;
        }
#endif
        s_hasPenTexture = false;
        s_lastCapValid = false;
    }
    s_penTargetActive = false;
    s_penVerts.clear();
    s_penVerts.shrink_to_fit();
}

void Renderer_NV2A::setRenderTarget(void *renderTarget, int width, int height) {
    (void)renderTarget;
    (void)width;
    (void)height;
}

void Renderer_NV2A::clearRenderTarget() {
    ensureMainTarget();
}

bool Renderer_NV2A::createSpeechManager() {
    if (!speechManager) speechManager = new SpeechManagerNV2A();
    return speechManager != nullptr;
}

SpeechManager *Renderer_NV2A::getSpeechManager() {
    if (!speechManager) speechManager = new SpeechManagerNV2A();
    return speechManager;
}

void Renderer_NV2A::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

int Renderer_NV2A::getWidth() {
    if (window) return window->getWidth();
    return 640;
}

int Renderer_NV2A::getHeight() {
    if (window) return window->getHeight();
    return 480;
}

float Renderer_NV2A::getPixelDensity() {
    return 1.0f;
}

void Renderer_NV2A::beginFrame(int screen, int colorR, int colorG, int colorB) {
    (void)screen;
    if (!hasFrameBegan) {
        ensureMainTarget();
        ++s_frameCounter;

#ifdef __XBOX__
        pb_wait_for_vbl();

        while (pb_busy()) {
            __asm__ __volatile__("pause");
        }

        pb_reset();
        pb_target_back_buffer();
        pb_erase_depth_stencil_buffer(0, 0, pb_back_buffer_width(), pb_back_buffer_height());

        uint32_t clearColor = (static_cast<uint32_t>(colorR) << 16) |
                              (static_cast<uint32_t>(colorG) << 8) |
                              (static_cast<uint32_t>(colorB)) |
                              0xFF000000;
        pb_fill(0, 0, pb_back_buffer_width(), pb_back_buffer_height(), clearColor);

        uint32_t *p = pb_begin();
        p = xgu_set_viewport_scale(p, 1.0f, 1.0f, 1.0f, 1.0f);
        p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
        p = xgu_set_scissor_rect(p, false, s_stageScissorX, s_stageScissorY, s_stageScissorW, s_stageScissorH);
        pb_end(p);

        while (pb_busy()) {
            __asm__ __volatile__("pause");
        }
#endif

        s_activeTexture = nullptr;
        s_activeShader = -1;
        s_activeBlendMode = NV2ABlendMode::None;
        s_frameIndex = (s_frameIndex + 1) % 3;
        s_arenaBase = s_frameIndex * ARENA_PER_FRAME_SIZE;
        s_arenaOffset = s_arenaBase;

        hasFrameBegan = true;
    }
}

void Renderer_NV2A::endFrame(bool shouldFlush) {
    (void)shouldFlush;
    if (!hasFrameBegan) return;
    ensureMainTarget();

#ifdef __XBOX__
    while (pb_busy()) {
        Sleep(0);
    }

    while (pb_finished()) {
        Sleep(0);
    }

    while (pb_busy()) {
        Sleep(0);
    }
#endif

    hasFrameBegan = false;
}

void Renderer_NV2A::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    ensureMainTarget();
    float x0 = static_cast<float>(x - w / 2.0f);
    float y0 = static_cast<float>(y - h / 2.0f);
    float x1 = x0 + static_cast<float>(w);
    float y1 = y0 + static_cast<float>(h);

    NV2AVertex verts[6] = {
        {{x0, y0}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}},
        {{x1, y0}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}},
        {{x1, y1}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}},
        {{x0, y0}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}},
        {{x1, y1}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}},
        {{x0, y1}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}}
    };

    setBlendMode(colorA < 255 ? NV2ABlendMode::Blend : NV2ABlendMode::None);
    drawColorTriangles(verts, 6);
}

void Renderer_NV2A::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    ensureMainTarget();

    const float left = x - w / 2.0f, right = x + w / 2.0f;
    const float top = y - h / 2.0f, bottom = y + h / 2.0f;
    const float r = static_cast<float>(radius);

    constexpr int cornerSegments = 4;
    std::vector<std::pair<float, float>> outline;
    outline.reserve((cornerSegments + 1) * 4);

    auto addArc = [&](float cx, float cy, float startAngle) {
        for (int i = 0; i <= cornerSegments; ++i) {
            float a = startAngle + (static_cast<float>(M_PI) / 2.0f) * (i / static_cast<float>(cornerSegments));
            outline.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
        }
    };

    addArc(right - r, top + r, -static_cast<float>(M_PI) / 2.0f);
    addArc(right - r, bottom - r, 0.0f);
    addArc(left + r, bottom - r, static_cast<float>(M_PI) / 2.0f);
    addArc(left + r, top + r, static_cast<float>(M_PI));

    std::vector<NV2AVertex> verts;
    verts.reserve((outline.size() - 2) * 3);
    for (size_t i = 1; i + 1 < outline.size(); ++i) {
        verts.push_back({{outline[0].first, outline[0].second}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}});
        verts.push_back({{outline[i].first, outline[i].second}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}});
        verts.push_back({{outline[i + 1].first, outline[i + 1].second}, {colorR, colorG, colorB, colorA}, {0.0f, 0.0f}});
    }

    setBlendMode(colorA < 255 ? NV2ABlendMode::Blend : NV2ABlendMode::None);
    drawColorTriangles(verts.data(), verts.size());
}

void Renderer_NV2A::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!s_hasPenTexture) return;
    s_lastCapValid = false;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    const double scale = s_penScaleY;
    float sx1 = static_cast<float>(x1 * scale + s_penTexture.texWidth / 2.0);
    float sy1 = static_cast<float>(-y1 * scale + s_penTexture.texHeight / 2.0);
    float sx2 = static_cast<float>(x2 * scale + s_penTexture.texWidth / 2.0);
    float sy2 = static_cast<float>(-y2 * scale + s_penTexture.texHeight / 2.0);

    double dx = sx2 - sx1, dy = sy2 - sy1;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0) return;

    double drawW = (sprite->penData.size / 2.0) * scale;
    float nx = static_cast<float>((-dy / len) * drawW);
    float ny = static_cast<float>((dx / len) * drawW);

    uint8_t ca = static_cast<uint8_t>(alpha * 255);
    uint8_t cr = static_cast<uint8_t>(std::clamp(col.r, 0.0f, 255.0f));
    uint8_t cg = static_cast<uint8_t>(std::clamp(col.g, 0.0f, 255.0f));
    uint8_t cb = static_cast<uint8_t>(std::clamp(col.b, 0.0f, 255.0f));

    NV2AVertex v0 = {{sx1 + nx, sy1 + ny}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v1 = {{sx1 - nx, sy1 - ny}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v2 = {{sx2 + nx, sy2 + ny}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v3 = {{sx2 - nx, sy2 - ny}, {cr, cg, cb, ca}, {0, 0}};

    s_penVerts.push_back(v0);
    s_penVerts.push_back(v1);
    s_penVerts.push_back(v2);

    s_penVerts.push_back(v1);
    s_penVerts.push_back(v3);
    s_penVerts.push_back(v2);

    if (s_penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(this);
    }
}

void Renderer_NV2A::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!s_hasPenTexture) return;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;
    uint8_t ca = static_cast<uint8_t>(alpha * 255);
    uint8_t cr = static_cast<uint8_t>(std::clamp(col.r, 0.0f, 255.0f));
    uint8_t cg = static_cast<uint8_t>(std::clamp(col.g, 0.0f, 255.0f));
    uint8_t cb = static_cast<uint8_t>(std::clamp(col.b, 0.0f, 255.0f));

    const double scale = s_penScaleY;
    float sx1 = static_cast<float>(x1 * scale + s_penTexture.texWidth / 2.0);
    float sy1 = static_cast<float>(-y1 * scale + s_penTexture.texHeight / 2.0);
    float sx2 = static_cast<float>(x2 * scale + s_penTexture.texWidth / 2.0);
    float sy2 = static_cast<float>(-y2 * scale + s_penTexture.texHeight / 2.0);

    double dx = sx2 - sx1, dy = sy2 - sy1;
    double len = std::sqrt(dx * dx + dy * dy);
    double drawW = (sprite->penData.size / 2.0) * scale;

    if (len > 0) {
        float nx = static_cast<float>((-dy / len) * drawW);
        float ny = static_cast<float>((dx / len) * drawW);

        NV2AVertex v0 = {{sx1 + nx, sy1 + ny}, {cr, cg, cb, ca}, {0, 0}};
        NV2AVertex v1 = {{sx1 - nx, sy1 - ny}, {cr, cg, cb, ca}, {0, 0}};
        NV2AVertex v2 = {{sx2 + nx, sy2 + ny}, {cr, cg, cb, ca}, {0, 0}};
        NV2AVertex v3 = {{sx2 - nx, sy2 - ny}, {cr, cg, cb, ca}, {0, 0}};

        s_penVerts.push_back(v0);
        s_penVerts.push_back(v1);
        s_penVerts.push_back(v2);

        s_penVerts.push_back(v1);
        s_penVerts.push_back(v3);
        s_penVerts.push_back(v2);
    }

    float radius = static_cast<float>(drawW);
    unsigned int segments = circleSegmentsFor(8.0, sprite->penData.size);
    const CirclePoint *table = getCircleTable(segments);

    auto addCircle = [&](float cx, float cy) {
        for (unsigned int i = 0; i < segments; ++i) {
            s_penVerts.push_back({{cx, cy}, {cr, cg, cb, ca}, {0, 0}});
            s_penVerts.push_back({{cx + table[i].c * radius, cy + table[i].s * radius}, {cr, cg, cb, ca}, {0, 0}});
            s_penVerts.push_back({{cx + table[i + 1].c * radius, cy + table[i + 1].s * radius}, {cr, cg, cb, ca}, {0, 0}});
        }
    };

    const uint32_t colorPacked = (static_cast<uint32_t>(ca) << 24) |
                                 (static_cast<uint32_t>(cr) << 16) |
                                 (static_cast<uint32_t>(cg) << 8) |
                                 static_cast<uint32_t>(cb);

    const bool continuesChain = alpha >= 1.0f && s_lastCapValid &&
                                s_lastCapX == sx1 && s_lastCapY == sy1 &&
                                s_lastCapRadius == radius && s_lastCapColor == colorPacked;

    if (!continuesChain) addCircle(sx1, sy1);
    addCircle(sx2, sy2);

    s_lastCapValid = true;
    s_lastCapX = sx2;
    s_lastCapY = sy2;
    s_lastCapRadius = radius;
    s_lastCapColor = colorPacked;

    if (s_penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(this);
    }
}

void Renderer_NV2A::penDotFast(Sprite *sprite) {
    if (!s_hasPenTexture) return;
    s_lastCapValid = false;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;
    uint8_t ca = static_cast<uint8_t>(alpha * 255);
    uint8_t cr = static_cast<uint8_t>(std::clamp(col.r, 0.0f, 255.0f));
    uint8_t cg = static_cast<uint8_t>(std::clamp(col.g, 0.0f, 255.0f));
    uint8_t cb = static_cast<uint8_t>(std::clamp(col.b, 0.0f, 255.0f));

    const double scale = s_penScaleY;
    float sx = static_cast<float>(sprite->xPosition * scale + s_penTexture.texWidth / 2.0);
    float sy = static_cast<float>(-sprite->yPosition * scale + s_penTexture.texHeight / 2.0);
    float halfSize = static_cast<float>((sprite->penData.size / 2.0) * scale);

    NV2AVertex v0 = {{sx - halfSize, sy - halfSize}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v1 = {{sx - halfSize, sy + halfSize}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v2 = {{sx + halfSize, sy - halfSize}, {cr, cg, cb, ca}, {0, 0}};
    NV2AVertex v3 = {{sx + halfSize, sy + halfSize}, {cr, cg, cb, ca}, {0, 0}};

    s_penVerts.push_back(v0);
    s_penVerts.push_back(v1);
    s_penVerts.push_back(v2);

    s_penVerts.push_back(v1);
    s_penVerts.push_back(v3);
    s_penVerts.push_back(v2);

    if (s_penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(this);
    }
}

void Renderer_NV2A::penDotAccurate(Sprite *sprite) {
    if (!s_hasPenTexture) return;
    s_lastCapValid = false;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;
    uint8_t ca = static_cast<uint8_t>(alpha * 255);
    uint8_t cr = static_cast<uint8_t>(std::clamp(col.r, 0.0f, 255.0f));
    uint8_t cg = static_cast<uint8_t>(std::clamp(col.g, 0.0f, 255.0f));
    uint8_t cb = static_cast<uint8_t>(std::clamp(col.b, 0.0f, 255.0f));

    const double scale = s_penScaleY;
    float sx = static_cast<float>(sprite->xPosition * scale + s_penTexture.texWidth / 2.0);
    float sy = static_cast<float>(-sprite->yPosition * scale + s_penTexture.texHeight / 2.0);
    float radius = static_cast<float>((sprite->penData.size / 2.0) * scale);

    unsigned int segments = circleSegmentsFor(16.0, sprite->penData.size);
    const CirclePoint *table = getCircleTable(segments);

    for (unsigned int i = 0; i < segments; ++i) {
        s_penVerts.push_back({{sx, sy}, {cr, cg, cb, ca}, {0, 0}});
        s_penVerts.push_back({{sx + table[i].c * radius, sy + table[i].s * radius}, {cr, cg, cb, ca}, {0, 0}});
        s_penVerts.push_back({{sx + table[i + 1].c * radius, sy + table[i + 1].s * radius}, {cr, cg, cb, ca}, {0, 0}});
    }

    if (s_penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(this);
    }
}

void Renderer_NV2A::penStamp(Sprite *sprite) {
    if (!s_hasPenTexture) return;
    flushPenVerts(this);
    s_lastCapValid = false;

    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) return;

    const Costume &costume = sprite->costumes[sprite->currentCostume];
    ensurePenTarget();

    Image_NV2A *image = reinterpret_cast<Image_NV2A *>(imgFind->second.get());
    Render::calculateRenderPosition(sprite, costume.isSVG);

    const auto &coords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    float penX = coords.first + Scratch::projectWidth / 2.0f;
    float penY = -coords.second + Scratch::projectHeight / 2.0f;

    float penScale;
    if (Scratch::hqpen) {
        penX *= s_penScaleY;
        penY *= s_penScaleY;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
        if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            penScale *= 2.0f;
        }
    }

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.scale = penScale;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;
    params.colorEffect = sprite->colorEffect;
    params.fisheyeEffect = sprite->fisheyeEffect;
    params.whirlEffect = sprite->whirlEffect;
    params.pixelateEffect = sprite->pixelateEffect;
    params.mosaicEffect = sprite->mosaicEffect;

    image->render(params);
}

void Renderer_NV2A::penClear() {
    if (!s_hasPenTexture) return;
    s_penVerts.clear();
    s_lastCapValid = false;

    ensurePenTarget();
    pb_fill(0, 0, s_penTexture.texWidth, s_penTexture.texHeight, 0x00000000);
}

void Renderer_NV2A::renderPenLayer() {
    if (!s_hasPenTexture) return;
    flushPenVerts(this);
    ensureMainTarget();

    float screenW = static_cast<float>(getWidth());
    float screenH = static_cast<float>(getHeight());
    float renderX = 0, renderY = 0, renderW = screenW, renderH = screenH;

    if (screenW / screenH > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderX = std::ceil((screenW - Scratch::projectWidth * (screenH / Scratch::projectHeight)) / 2.0f);
        renderW = screenW - renderX * 2;
    } else {
        renderY = std::ceil((screenH - Scratch::projectHeight * (screenW / Scratch::projectWidth)) / 2.0f);
        renderH = screenH - renderY * 2;
    }

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = s_penTexture.uScale;
    float v1 = s_penTexture.vScale;

    NV2AVertex quad[6] = {
        {{renderX, renderY}, {255, 255, 255, 255}, {u0, v0}},
        {{renderX + renderW, renderY}, {255, 255, 255, 255}, {u1, v0}},
        {{renderX + renderW, renderY + renderH}, {255, 255, 255, 255}, {u1, v1}},
        {{renderX, renderY}, {255, 255, 255, 255}, {u0, v0}},
        {{renderX + renderW, renderY + renderH}, {255, 255, 255, 255}, {u1, v1}},
        {{renderX, renderY + renderH}, {255, 255, 255, 255}, {u0, v1}}
    };

    bindTexture(&s_penTexture, false);
    setBlendMode(NV2ABlendMode::Blend);
    drawTriangles(quad, 6);
}

static void drawBlackBarsNV2A(Renderer_NV2A *r, int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        r->drawBox(static_cast<int>(std::ceil(barWidth)), screenHeight, static_cast<int>(barWidth / 2.0f), screenHeight / 2, 0, 0, 0, 255);
        r->drawBox(static_cast<int>(std::ceil(barWidth)), screenHeight, static_cast<int>(screenWidth - barWidth / 2.0f), screenHeight / 2, 0, 0, 0, 255);
    } else if (screenAspect < projectAspect) {
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        r->drawBox(screenWidth, static_cast<int>(std::ceil(barHeight)), screenWidth / 2, static_cast<int>(barHeight / 2.0f), 0, 0, 0, 255);
        r->drawBox(screenWidth, static_cast<int>(std::ceil(barHeight)), screenWidth / 2, static_cast<int>(screenHeight - barHeight / 2.0f), 0, 0, 0, 255);
    }
}

void Renderer_NV2A::renderSprites() {
    int expectedPenW = Scratch::hqpen ? getWidth() : Scratch::projectWidth;
    int expectedPenH = Scratch::hqpen ? getHeight() : Scratch::projectHeight;
    if (s_hasPenTexture && (s_penTexture.texWidth != expectedPenW || s_penTexture.texHeight != expectedPenH)) {
        cleanupPen();
        initPen();
    }

    beginFrame(0, 0, 0, 0);

    float screenW = static_cast<float>(getWidth());
    float screenH = static_cast<float>(getHeight());
    float screenAspect = screenW / screenH;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float stageX = 0, stageY = 0, stageW = screenW, stageH = screenH;
    if (screenAspect > projectAspect) {
        float scale = screenH / Scratch::projectHeight;
        stageW = Scratch::projectWidth * scale;
        stageX = (screenW - stageW) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scale = screenW / Scratch::projectWidth;
        stageH = Scratch::projectHeight * scale;
        stageY = (screenH - stageH) / 2.0f;
    }

    s_stageScissorX = static_cast<int>(std::round(stageX * s_mainViewportScaleX + s_mainViewportOffsetX));
    s_stageScissorY = static_cast<int>(std::round(stageY * s_mainViewportScaleY + s_mainViewportOffsetY));
    s_stageScissorW = static_cast<int>(std::round(stageW * s_mainViewportScaleX));
    s_stageScissorH = static_cast<int>(std::round(stageH * s_mainViewportScaleY));

    clearScissor();

    drawBox(static_cast<int>(stageW), static_cast<int>(stageH),
            static_cast<int>(stageX + stageW / 2.0f), static_cast<int>(stageY + stageH / 2.0f),
            255, 255, 255, 255);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image *image = imgFind->second.get();
            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            Render::calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            ImageRenderParams params;
            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);
            params.opacity = 1.0f - (std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
            params.brightness = currentSprite->brightnessEffect;
            params.colorEffect = currentSprite->colorEffect;
            params.fisheyeEffect = currentSprite->fisheyeEffect;
            params.whirlEffect = currentSprite->whirlEffect;
            params.pixelateEffect = currentSprite->pixelateEffect;
            params.mosaicEffect = currentSprite->mosaicEffect;

            image->render(params);
        }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBarsNV2A(this, getWidth(), getHeight());
    Render::renderMonitors();

    if (Input::mousePointer.isMoving) {
        int mx = static_cast<int>((Input::mousePointer.x * Render::renderScale) + (getWidth() * 0.5f));
        int my = static_cast<int>((Input::mousePointer.y * -1 * Render::renderScale) + (getHeight() * 0.5f));
        drawBox(6, 6, mx, my, 0, 120, 255, 255);
    }

    endFrame(true);
}

bool Renderer_NV2A::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        ensureMainTarget();
        window->pollEvents();
        return !window->shouldClose();
    }
    return false;
}
