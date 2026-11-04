#include "se_exec_arena.h"
#include <psp2/kernel/sysmem.h>

#define SE_VITA_JIT_SIZE (4u << 20)

static se_exec_arena arena;
static SceUID block = 0;
static int ready = 0;
static int executable = 0;

static int ensure(void) {
    if (ready) return 1;
    SceUID uid = sceKernelAllocMemBlockForVM("se_jit", SE_VITA_JIT_SIZE);
    if (uid < 0) return 0;
    void *base = NULL;
    if (sceKernelGetMemBlockBase(uid, &base) < 0 || base == NULL || !se_exec_arena_init(&arena, base, SE_VITA_JIT_SIZE)) {
        sceKernelFreeMemBlock(uid);
        return 0;
    }
    if (sceKernelOpenVMDomain() < 0) {
        se_exec_arena_destroy(&arena);
        sceKernelFreeMemBlock(uid);
        return 0;
    }
    block = uid;
    ready = 1;
    executable = 0;
    return 1;
}

void *se_sljit_malloc_exec(unsigned long size) {
    if (!ensure()) return NULL;
    if (executable) {
        if (sceKernelOpenVMDomain() < 0) return NULL;
        executable = 0;
    }
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

void se_sljit_update_wx(void *from, void *to, int want_executable) {
    (void)from;
    (void)to;
    if (!ready) return;
    if (want_executable && !executable) {
        if (sceKernelCloseVMDomain() >= 0) executable = 1;
    } else if (!want_executable && executable) {
        if (sceKernelOpenVMDomain() >= 0) executable = 0;
    }
}

void se_sljit_cache_flush(void *from, void *to) {
    if (!ready) return;
    sceKernelSyncVMDomain(block, from, (SceSize)((uint8_t *)to - (uint8_t *)from));
}
