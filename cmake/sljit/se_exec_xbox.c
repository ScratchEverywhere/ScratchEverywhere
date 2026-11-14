#include "se_exec_arena.h"
#include <stdlib.h>

#define SE_XBOX_JIT_SIZE (1u << 20) /* 1 MB */

static se_exec_arena arena;
static int ready = 0;

static int ensure(void) {
    if (ready) return 1;
    void *memory = malloc(SE_XBOX_JIT_SIZE);
    if (memory == NULL) return 0;
    if (!se_exec_arena_init(&arena, memory, SE_XBOX_JIT_SIZE)) {
        free(memory);
        return 0;
    }
    ready = 1;
    return 1;
}

void *se_sljit_malloc_exec(unsigned long size) {
    if (!ensure()) return NULL;
    return se_exec_arena_alloc(&arena, size);
}

void se_sljit_free_exec(void *ptr) {
    if (!ready || ptr == NULL) return;
    se_exec_arena_free(&arena, (size_t)((uint8_t *)ptr - arena.base));
}

long se_sljit_exec_offset(void *ptr) {
    (void)ptr;
    return 0;
}

void se_sljit_update_wx(void *from, void *to, int executable) {
    (void)from;
    (void)to;
    (void)executable;
}

void se_sljit_cache_flush(void *from, void *to) {
    (void)from;
    (void)to;
}
