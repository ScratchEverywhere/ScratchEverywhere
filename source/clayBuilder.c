// This prevents Clay being built multiple times.

#ifdef ENABLE_MENU
#define CLAY_IMPLEMENTATION
#include <clay.h>
#endif
