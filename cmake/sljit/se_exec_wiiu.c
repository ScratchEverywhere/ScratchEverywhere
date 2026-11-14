#include "se_exec_arena.h"
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/event.h>
#include <coreinit/thread.h>

static se_exec_arena arena;
static int ready = 0;
static int executable = 0;

#define SE_WIIU_HELPER_STACK (16 * 1024)

static OSThread helperThread;
static uint8_t helperStack[SE_WIIU_HELPER_STACK] __attribute__((aligned(16)));
static OSEvent requestEvent;
static OSEvent doneEvent;
static volatile int helperRunning = 0;
static volatile OSCodegenSecMode requestedMode;
static volatile int requestResult;

static int helperMain(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    for (;;) {
        OSWaitEvent(&requestEvent);
        requestResult = OSSwitchSecCodeGenMode(requestedMode) ? 1 : 0;
        OSSignalEvent(&doneEvent);
    }
    return 0;
}

static int start_helper(void) {
    if (helperRunning) return 1;
    const uint32_t core = OSGetCodegenCore();
    if (core > 2) {
        se_exec_fail("unexpected codegen core");
        return 0;
    }
    OSInitEvent(&requestEvent, false, OS_EVENT_MODE_AUTO);
    OSInitEvent(&doneEvent, false, OS_EVENT_MODE_AUTO);
    if (!OSCreateThread(&helperThread, helperMain, 0, NULL, helperStack + SE_WIIU_HELPER_STACK, SE_WIIU_HELPER_STACK, 16,
                        (OSThreadAttributes)(1u << core))) {
        se_exec_fail("could not create the codegen helper thread");
        return 0;
    }
    OSSetThreadName(&helperThread, "se-codegen");
    OSResumeThread(&helperThread);
    helperRunning = 1;
    return 1;
}

static int switch_mode(OSCodegenSecMode mode) {
    int ok;
    if (OSGetCoreId() == OSGetCodegenCore()) {
        ok = OSSwitchSecCodeGenMode(mode) ? 1 : 0;
    } else {
        if (!start_helper()) return 0;
        requestedMode = mode;
        OSSignalEvent(&requestEvent);
        OSWaitEvent(&doneEvent);
        ok = requestResult;
    }
    if (!ok) se_exec_fail("OSSwitchSecCodeGenMode failed (is codegen enabled in cos.xml?)");
    return ok;
}

static int ensure(void) {
    if (ready) return 1;
    void *address = NULL;
    uint32_t size = 0;
    OSGetCodegenVirtAddrRange(&address, &size);
    if (address == NULL || size == 0) {
        se_exec_fail("no codegen area (is codegen enabled in cos.xml?)");
        return 0;
    }
    if (!switch_mode(CODEGEN_RW_)) return 0;
    if (!se_exec_arena_init(&arena, address, size)) {
        se_exec_fail("could not set up the codegen arena");
        return 0;
    }
    ready = 1;
    executable = 0;
    return 1;
}

void *se_sljit_malloc_exec(unsigned long size) {
    if (!ensure()) return NULL;
    if (executable) {
        if (!switch_mode(CODEGEN_RW_)) return NULL;
        executable = 0;
    }
    void *ptr = se_exec_arena_alloc(&arena, size);
    if (ptr == NULL) se_exec_fail("codegen area is full");
    return ptr;
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
        if (switch_mode(CODEGEN_R_X)) executable = 1;
    } else if (!want_executable && executable) {
        if (switch_mode(CODEGEN_RW_)) executable = 0;
    }
}

void se_sljit_cache_flush(void *from, void *to) {
    (void)from;
    (void)to;
}
