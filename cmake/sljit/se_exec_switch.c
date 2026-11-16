#include "se_exec_arena.h"
#include <stdio.h>
#include <switch.h>

#define SE_SWITCH_JIT_SIZE (8u << 20)

static Jit jit;
static se_exec_arena arena;
static char failure[96];
static int ready = 0;
static int executable = 0;

static int ensure(void) {
    if (ready) return 1;
    const Result rc = jitCreate(&jit, SE_SWITCH_JIT_SIZE);
    if (R_FAILED(rc)) {
        snprintf(failure, sizeof(failure), "jitCreate failed: module %u description %u (not launched as a title/application?)", (unsigned)R_MODULE(rc), (unsigned)R_DESCRIPTION(rc));
        se_exec_fail(failure);
        return 0;
    }
    if (!se_exec_arena_init(&arena, jitGetRwAddr(&jit), SE_SWITCH_JIT_SIZE)) {
        jitClose(&jit);
        se_exec_fail("could not set up the JIT arena");
        return 0;
    }
    ready = 1;
    executable = 0;
    return 1;
}

static long alias_offset(void) {
    return (long)((uint8_t *)jitGetRxAddr(&jit) - (uint8_t *)jitGetRwAddr(&jit));
}

void *se_sljit_malloc_exec(unsigned long size) {
    if (!ensure()) return NULL;
    if (executable) {
        if (R_FAILED(jitTransitionToWritable(&jit))) {
            se_exec_fail("jitTransitionToWritable failed");
            return NULL;
        }
        executable = 0;
    }
    void *ptr = se_exec_arena_alloc(&arena, size);
    if (ptr == NULL) se_exec_fail("JIT memory is full");
    return ptr;
}

void se_sljit_free_exec(void *ptr) {
    if (!ready || ptr == NULL) return;
    uint8_t *writable = (uint8_t *)ptr;
    if (!se_exec_arena_contains(&arena, ptr)) writable -= alias_offset();
    se_exec_arena_free(&arena, (size_t)(writable - arena.base));
}

long se_sljit_exec_offset(void *ptr) {
    (void)ptr;
    return ready ? alias_offset() : 0;
}

void se_sljit_update_wx(void *from, void *to, int want_executable) {
    (void)from;
    (void)to;
    if (!ready) return;
    if (want_executable && !executable) {
        if (R_SUCCEEDED(jitTransitionToExecutable(&jit))) executable = 1;
    } else if (!want_executable && executable) {
        if (R_SUCCEEDED(jitTransitionToWritable(&jit))) executable = 0;
    }
}

void se_sljit_cache_flush(void *from, void *to) {
    (void)from;
    (void)to;
}
