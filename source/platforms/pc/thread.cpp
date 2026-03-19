#include <chrono>
#include <mutex>
#include <pthread.h>
#include <thread.hpp>
#include <thread>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

struct SE_Thread::Impl {
    pthread_t thread;
    bool active = false;
};

struct PthreadData {
    void (*entryPoint)(void *);
    void *args;
};

static void *pthread_Wrapper(void *data) {
    PthreadData *ctx = static_cast<PthreadData *>(data);
    ctx->entryPoint(ctx->args);
    delete ctx;
    return nullptr;
}

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    if (impl != nullptr) return false;

    impl = new Impl;

    PthreadData *data = new PthreadData;
    data->entryPoint = entryPoint;
    data->args = args;

    int result = pthread_create(&impl->thread, nullptr, pthread_Wrapper, data);

    if (result != 0) {
        delete data;
        delete impl;
        impl = nullptr;
        return false;
    }

    impl->active = true;
    return true;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr && impl->active) {
        pthread_join(impl->thread, nullptr);
        impl->active = false;
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl != nullptr && impl->active) {
        pthread_detach(impl->thread);
        impl->active = false;
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    usleep(milliseconds * 1000);
}

unsigned int SE_Thread::getCurrentThreadId() {
    return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(pthread_self()));
}

struct SE_Mutex::Impl {
#ifdef _WIN32
    CRITICAL_SECTION mtx;
#else
    std::mutex mtx;
#endif
};

SE_Mutex::SE_Mutex() {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;
#ifdef _WIN32
        InitializeCriticalSection(&impl->mtx);
#endif
    }
}

SE_Mutex::~SE_Mutex() {
    if (impl) {
#ifdef _WIN32
        DeleteCriticalSection(&impl->mtx);
#endif
        delete impl;
        impl = nullptr;
    }
}

void SE_Mutex::lock() {
#ifdef _WIN32
    EnterCriticalSection(&impl->mtx);
#else
    impl->mtx.lock();
#endif
}

void SE_Mutex::unlock() {
#ifdef _WIN32
    LeaveCriticalSection(&impl->mtx);
#else
    impl->mtx.unlock();
#endif
}

bool SE_Mutex::tryLock() {
#ifdef _WIN32
    return TryEnterCriticalSection(&impl->mtx) != 0;
#else
    return impl->mtx.try_lock();
#endif
}