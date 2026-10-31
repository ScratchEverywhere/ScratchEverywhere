#include <thread.hpp>
#include <lv2/mutex.h>
#include <unistd.h>

struct SE_Thread::Impl {};

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    (void)entryPoint;
    (void)args;
    (void)stackSize;
    (void)prio;
    (void)coreID;
    (void)name;
    return false;
}

void SE_Thread::join() {
}

void SE_Thread::detach() {
}

void SE_Thread::sleep(uint16_t milliseconds) {
    usleep(milliseconds * 1000);
}

unsigned int SE_Thread::getCurrentThreadId() {
    return 0;
}

SE_Thread::~SE_Thread() {
    if (impl) delete impl;
}

struct SE_Mutex::Impl {
    sys_lwmutex_t mutex{};
    bool initialized = false;
};

SE_Mutex::SE_Mutex() : impl(nullptr) {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;
        sys_lwmutex_attr_t attr{};
        attr.attr_protocol = SYS_LWMUTEX_PROTOCOL_PRIO_INHERIT;
        attr.attr_recursive = SYS_LWMUTEX_ATTR_NOT_RECURSIVE;
        sysLwMutexCreate(&impl->mutex, &attr);
        impl->initialized = true;
    }
}

SE_Mutex::~SE_Mutex() {
    if (impl) {
        if (impl->initialized) sysLwMutexDestroy(&impl->mutex);
        delete impl;
        impl = nullptr;
    }
}

void SE_Mutex::lock() {
    if (impl && impl->initialized) sysLwMutexLock(&impl->mutex, 0);
}

void SE_Mutex::unlock() {
    if (impl && impl->initialized) sysLwMutexUnlock(&impl->mutex);
}

bool SE_Mutex::tryLock() {
    if (!impl || !impl->initialized) return true;
    return sysLwMutexTryLock(&impl->mutex) == 0;
}
