#ifdef RETROARCH
#include <libretro.h>

#include <cstring>

extern "C" {
unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Scratch Everywhere";
    info->library_version = "v1";
    info->need_fullpath = true;
    info->valid_extensions = "sb3|zip";
}
};
#endif
