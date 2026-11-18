#include "se_exec_arena.h"
#include <xsdk/jit.h>

extern char se_xbox360_jit_pool_anchor[];

static se_exec_arena arena;
static int ready = 0;

static int ensure(void) {
    if (ready) return 1;
    void * volatile anchor = se_xbox360_jit_pool_anchor;
    (void)anchor;
    void *base = NULL;
    size_t bytes = 0;
    xsdk_jit_pool(&base, &bytes);
    if (base == NULL || bytes == 0) {
        se_exec_fail("xsdk_jit_pool returned empty pool");
        return 0;
    }
    if (!se_exec_arena_init(&arena, base, bytes)) return 0;
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
    if (!ready || from == NULL || to <= from) return;
    xsdk_code_flush(from, (size_t)((const uint8_t *)to - (const uint8_t *)from));
}
