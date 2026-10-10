#include <chrono>
#include <mutex>
#include <process.h>
#include <thread.hpp>
#include <thread>
#include <unistd.h>
#include <windows.h>

struct SE_Thread::Impl {
    HANDLE thread;
    bool active = false;
};

struct ThreadData {
    void (*entryPoint)(void *);
    void *args;
};

static unsigned CALLBACK win32_Wrapper(void *data) {
    ThreadData *ctx = static_cast<ThreadData *>(data);
    ctx->entryPoint(ctx->args);
    delete ctx;
    _endthreadex(0);
    return 0;
}

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    if (impl != nullptr) return false;

    impl = new Impl;

    ThreadData *data = new ThreadData;
    data->entryPoint = entryPoint;
    data->args = args;

    unsigned id;
    impl->thread = (HANDLE)_beginthreadex(nullptr, 0, win32_Wrapper, data, 0, &id);

    if (impl->thread == (HANDLE)-1) {
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
    if (impl != nullptr) {
        if (impl->active) {
            WaitForSingleObject(impl->thread, INFINITE);
            CloseHandle(impl->thread);
            impl->active = false;
        }
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl != nullptr && impl->active) {
        CloseHandle(impl->thread);
        impl->active = false;
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    Sleep(milliseconds);
}

unsigned int SE_Thread::getCurrentThreadId() {
    return static_cast<unsigned int>((uintptr_t)GetCurrentThread());
}

struct SE_Mutex::Impl {
    CRITICAL_SECTION mtx;
};

SE_Mutex::SE_Mutex() {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;
        InitializeCriticalSection(&impl->mtx);
    }
}

SE_Mutex::~SE_Mutex() {
    if (impl) {
        DeleteCriticalSection(&impl->mtx);
        delete impl;
        impl = nullptr;
    }
}

void SE_Mutex::lock() {
    EnterCriticalSection(&impl->mtx);
}

void SE_Mutex::unlock() {
    LeaveCriticalSection(&impl->mtx);
}

bool SE_Mutex::tryLock() {
    return TryEnterCriticalSection(&impl->mtx) != 0;
}
