#ifdef LIBRETRO
#include <libretro.h>
#include <window.hpp>

extern WindowSE *globalWindow;

struct retro_hw_render_callback hw_render;
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

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Scratch Everywhere";
    info->library_version = "v1";
    info->need_fullpath = true;
    info->valid_extensions = "sb3|zip";
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_sample_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_sample_batch_cb = cb;
}

static int current_width = 540;
static int current_height = 405;

static void update_variables(void) {
    struct retro_variable var = {"scratch_internal_resolution", NULL};
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        int old_w = current_width;
        int old_h = current_height;

        if (strcmp(var.value, "1080x810") == 0) {
            current_width = 1080;
            current_height = 810;
        } else if (strcmp(var.value, "1620x1215") == 0) {
            current_width = 1620;
            current_height = 1215;
        } else if (strcmp(var.value, "2160x1620") == 0) {
            current_width = 2160;
            current_height = 1620;
        } else if (strcmp(var.value, "2880x2160") == 0) {
            current_width = 2880;
            current_height = 2160;
        } else {
            current_width = 540;
            current_height = 405;
        }

        if (old_w != current_width || old_h != current_height) {
            if (globalWindow) {
                globalWindow->resize(current_width, current_height);
            }

            struct retro_system_av_info av_info;
            retro_get_system_av_info(&av_info);
            environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
        }
    }
}

static void set_core_options(void) {
    static const struct retro_variable vars[] = {
        {"scratch_internal_resolution", "Internal Resolution; 540x405|1080x810|1620x1215|2160x1620|2880x2160"},
        {NULL, NULL}};
    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->timing.fps = 60.0;
    info->timing.sample_rate = Mixer::rate;

    info->geometry.base_width = current_width;
    info->geometry.base_height = current_height;
    info->geometry.max_width = 3840;
    info->geometry.max_height = 2160;
    info->geometry.aspect_ratio = 4.0 / 3.0;
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    set_core_options();
}

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        update_variables();
    }

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

    update_variables();

    if (!Render::Init()) return;

    if (!Unzip::load()) return;

    Scratch::initializeRuntime();

    Scratch::initializeScratchProject();
}

static void context_destroy(void) {
    Render::deInit();
}

static bool init_hw_context(void) {
#ifdef RENDERER_OPENGL
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#elif defined(RENDERER_OPENGL_CORE)
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    hw_render.version_major = 4;
    hw_render.version_minor = 1;
#endif

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
