#ifdef LIBRETRO
#include <libretro.h>

static struct retro_hw_render_callback hw_render;
static retro_audio_sample_t audio_sample_cb;
static retro_audio_sample_batch_t audio_sample_batch_cb;
static retro_environment_t environ_cb;

#include <cstring>

#include <audiostack.hpp>
#include <render.hpp>
#include <renderers/opengl/render.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

#ifdef ENABLE_AUDIO
#include <audio.hpp>
#endif

#define WIDTH 540
#define HEIGHT 405

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif

static ScriptThread monitorDisplayThread;

typedef void(APIENTRYP SE_PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
static SE_PFNGLBINDFRAMEBUFFERPROC se_glBindFramebuffer;

extern "C" {
void retro_init() {
    srand(time(NULL));
}

void retro_deinit() {
}

unsigned retro_api_version() {
    return RETRO_API_VERSION;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->timing.fps = 0;
    info->timing.sample_rate = Mixer::rate;

    info->geometry.base_width = WIDTH;
    info->geometry.base_height = HEIGHT;
    info->geometry.max_width = 2048;
    info->geometry.max_height = 2048;
    info->geometry.aspect_ratio = 4.0 / 3.0;
}

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Scratch Everywhere";
    info->library_version = "v1";
    info->need_fullpath = true;
    info->valid_extensions = "sb3|zip";
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_sample_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_sample_batch_cb = cb;
}

void retro_run(void) {
    std::pair<bool, bool> code;
    int samples = 0.06 * Mixer::rate;
    short stream[samples * 2];
    int i;

    Mixer::requestSound(stream, samples);

    for (i = 0; i < samples; i++)
        audio_sample_cb(stream[2 * i + 0], stream[2 * i + 1]);

    se_glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());

    code = Scratch::stepScratchProject(monitorDisplayThread);
    if (!code.first) {
        /* uhhh idk what do i do here? */
    }
}

/* this actually stinks to do this here but libretro does not let me do this outside of this function */
static void context_reset(void) {
    se_glBindFramebuffer = (SE_PFNGLBINDFRAMEBUFFERPROC)hw_render.get_proc_address("glBindFramebuffer");

    if (!Unzip::load()) return;

    if (!Render::Init()) {
        return;
    }

#ifdef ENABLE_AUDIO
    if (!SoundPlayer::init()) {
        return;
    }
#endif

    Scratch::initializeScratchProject();
}

static void context_destroy(void) {
}

static bool init_hw_context(void) {
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
    hw_render.context_reset = context_reset;
    hw_render.context_destroy = context_destroy;
    hw_render.depth = false;
    hw_render.bottom_left_origin = true;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
        return false;

    return true;
}

bool retro_load_game(const struct retro_game_info *info) {
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    Unzip::filePath = info->path;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        fprintf(stderr, "XRGB8888 is not supported.\n");
        return false;
    }

    if (!init_hw_context()) {
        return false;
    }

    return true;
}

void retro_unload_game(void) {
    Scratch::cleanupScratchProject();
    Render::deInit();
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    return false;
}

unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC; /* what? */
}

size_t retro_serialize_size(void) {
    return 0;
}

bool retro_serialize(void *data, size_t size) {
    return false;
}

bool retro_unserialize(const void *data, size_t size) {
    return false;
}

void *retro_get_memory_data(unsigned id) {
    return NULL;
}

size_t retro_get_memory_size(unsigned id) {
    return 0;
}

void retro_cheat_reset(void) {
}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
}

void retro_reset(void) {
}
}
#endif
