#pragma once

#ifdef __PC__
#include <cstdint>

extern "C" uint8_t _binary_romfs_gfx_Arialn_ttf_start[];
extern "C" uint8_t _binary_romfs_gfx_Arialn_ttf_end[];

#ifdef PROJECT_EMBEDDED
extern "C" uint8_t _binary_romfs_project_sb3_start[];
extern "C" uint8_t _binary_romfs_project_sb3_end[];
#endif
#endif
