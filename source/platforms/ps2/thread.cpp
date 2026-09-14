#include "thread.hpp"
#include <kernel.h>
#include <malloc.h>
#include <string>
#include <unistd.h>

// PS2 threads/mutexes referenced from SDL3

void *_gp;

struct PS2_ThreadData {
    void (*entryPoint)(void *);
    void *args;
    int sema_id;
};

struct SE_Thread::Impl {
    int thread_id;
    int sema_id;
    void *stack;
};

static int PS2_ThreadWrapper(void *data) {
    PS2_ThreadData *ctx = static_cast<PS2_ThreadData *>(data);

    ctx->entryPoint(ctx->args);

    int sema = ctx->sema_id;
    delete ctx;

    SignalSema(sema);

    ExitThread();
    return 0;
}

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    impl = new Impl;
    impl->stack = nullptr;
    impl->thread_id = -1;
    impl->sema_id = -1;

    PS2_ThreadData *ctx = new PS2_ThreadData;
    ctx->entryPoint = entryPoint;
    ctx->args = args;

    ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    impl->sema_id = CreateSema(&sema);
    ctx->sema_id = impl->sema_id;

    if (impl->sema_id < 0) {
        delete ctx;
        delete impl;
        impl = nullptr;
        return false;
    }

    if (stackSize == 0) stackSize = 0x8000;

    impl->stack = memalign(16, stackSize);

    ee_thread_t eethread;
    eethread.attr = 0;
    eethread.option = 0;
    eethread.func = reinterpret_cast<void *>(PS2_ThreadWrapper);
    eethread.stack = impl->stack;
    eethread.stack_size = stackSize;
    eethread.gp_reg = &_gp;
    eethread.initial_priority = (prio > 0) ? prio : 32;

    impl->thread_id = CreateThread(&eethread);

    if (impl->thread_id < 0) {
        DeleteSema(impl->sema_id);
        free(impl->stack);
        delete ctx;
        delete impl;
        impl = nullptr;
        return false;
    }

    if (StartThread(impl->thread_id, ctx) < 0) {
        DeleteThread(impl->thread_id);
        DeleteSema(impl->sema_id);
        free(impl->stack);
        delete ctx;
        delete impl;
        impl = nullptr;
        return false;
    }

    return true;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr && impl->thread_id >= 0) {
        WaitSema(impl->sema_id);

        TerminateThread(impl->thread_id);
        DeleteThread(impl->thread_id);
        DeleteSema(impl->sema_id);

        if (impl->stack) {
            free(impl->stack);
        }

        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl != nullptr) {
        // oh beans PS2 threads have no detach function
        // just gonna join for now
        join();
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    usleep(milliseconds * 1000);
}

unsigned int SE_Thread::getCurrentThreadId() {
    return static_cast<unsigned int>(GetThreadId());
}

struct SE_Mutex::Impl {
    int sema_id;
};

SE_Mutex::SE_Mutex() : impl(nullptr) {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;

        ee_sema_t sema;
        sema.init_count = 1;
        sema.max_count = 1;
        sema.option = 0;
        impl->sema_id = CreateSema(&sema);
    }
}

SE_Mutex::~SE_Mutex() {
    if (impl) {
        DeleteSema(impl->sema_id);
        delete impl;
        impl = nullptr;
    }
}

void SE_Mutex::lock() {
    if (impl) {
        WaitSema(impl->sema_id);
    }
}

void SE_Mutex::unlock() {
    if (impl) {
        SignalSema(impl->sema_id);
    }
}

bool SE_Mutex::tryLock() {
    if (impl) {
        return PollSema(impl->sema_id) == 0;
    }
    return false;
}