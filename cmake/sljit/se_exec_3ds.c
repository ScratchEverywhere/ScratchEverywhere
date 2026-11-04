#include "se_exec_arena.h"
#include <3ds.h>
#include <malloc.h>

#define SE_3DS_JIT_SIZE (2u << 20)

static se_exec_arena arena;
static Handle process = 0;
static int ready = 0;

static int ensure(void) {
    if (ready) return 1;
    u32 pid = 0;
    if (R_FAILED(svcGetProcessId(&pid, CUR_PROCESS_HANDLE))) return 0;
    if (R_FAILED(svcOpenProcess(&process, pid))) return 0;
    void *memory = memalign(0x1000, SE_3DS_JIT_SIZE);
    if (memory == NULL) {
        svcCloseHandle(process);
        return 0;
    }
    if (R_FAILED(svcControlProcessMemory(process, (u32)memory, (u32)memory, SE_3DS_JIT_SIZE, MEMOP_PROT, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE)) ||
        !se_exec_arena_init(&arena, memory, SE_3DS_JIT_SIZE)) {
        free(memory);
        svcCloseHandle(process);
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
    if (!ready) return;
    svcFlushProcessDataCache(process, (u32)from, (u32)((uint8_t *)to - (uint8_t *)from));
}
