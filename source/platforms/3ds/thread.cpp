#include <3ds.h>
#include <algorithm>
#include <thread.hpp>

struct SE_Thread::Impl {
    Thread thread;
};

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    impl = new Impl;
    s32 mainPrio = 0;
    svcGetThreadPriority(&mainPrio, CUR_THREAD_HANDLE);

    // only using Old 3DS cores because i have no idea if New 3DS cores would even work
    coreID = std::clamp(coreID, -2, 1);
    if (coreID == 1 && !R_SUCCEEDED(APT_SetAppCpuTimeLimit(50 /* maybe should be customizable? */))) coreID = 0;

    impl->thread = threadCreate(entryPoint, args, stackSize, std::clamp(static_cast<int>(mainPrio + prio), 0x18, 0x3F), coreID, false);
    return impl->thread != NULL;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr && impl->thread != nullptr) {
        threadJoin(impl->thread, U64_MAX);
        threadFree(impl->thread);
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl->thread != nullptr) {
        threadDetach(impl->thread);
        impl->thread = nullptr;
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    svcSleepThread(static_cast<uint64_t>(milliseconds) * 1000000LL);
}

unsigned int SE_Thread::getCurrentThreadId() {
    uint32_t threadId = 0;
    svcGetThreadId(&threadId, CUR_THREAD_HANDLE);
    return static_cast<unsigned int>(threadId);
}

struct SE_Mutex::Impl {
    LightLock mtx;
};

SE_Mutex::SE_Mutex() {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;
        LightLock_Init(&impl->mtx);
    }
}

SE_Mutex::~SE_Mutex() {
    delete impl;
}

void SE_Mutex::lock() {
    LightLock_Lock(&impl->mtx);
}

void SE_Mutex::unlock() {
    LightLock_Unlock(&impl->mtx);
}

bool SE_Mutex::tryLock() {
    return (LightLock_TryLock(&impl->mtx) == 0);
}
