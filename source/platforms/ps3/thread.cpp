#include <chrono>
#include <mutex>
#include <thread.hpp>
#include <thread>
#include <unistd.h>
#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
extern "C" {
#include <pthread.h>
}
#endif

struct SE_Thread::Impl {
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
    bool active = false;
};

struct ThreadData {
    void (*entryPoint)(void *);
    void *args;
};
#ifdef _WIN32
static unsigned CALLBACK win32_Wrapper(void *data) {
    ThreadData *ctx = static_cast<ThreadData *>(data);
    ctx->entryPoint(ctx->args);
    delete ctx;
    _endthreadex(0);
    return 0;
}
#else
static void *pthread_Wrapper(void *data) {
    ThreadData *ctx = static_cast<ThreadData *>(data);
    ctx->entryPoint(ctx->args);
    delete ctx;
    return nullptr;
}
#endif

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    if (impl != nullptr) return false;

    impl = new Impl;

    ThreadData *data = new ThreadData;
    data->entryPoint = entryPoint;
    data->args = args;

#ifdef _WIN32
    unsigned id;
    impl->thread = (HANDLE)_beginthreadex(nullptr, 0, win32_Wrapper, data, 0, &id);

    /* ive never seen it fail so i am not sure if this is right. MSDN says -1 is the failure */
    if (impl->thread == (HANDLE)-1) {
        delete data;
        delete impl;
        impl = nullptr;
        return false;
    }
#else
    int result = pthread_create(&impl->thread, nullptr, pthread_Wrapper, data);

    if (result != 0) {
        delete data;
        delete impl;
        impl = nullptr;
        return false;
    }
#endif

    impl->active = true;
    return true;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr) {
        if (impl->active) {
#ifdef _WIN32
            WaitForSingleObject(impl->thread, INFINITE);
            CloseHandle(impl->thread);
#else
            pthread_join(impl->thread, nullptr);
#endif
            impl->active = false;
        }
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl != nullptr && impl->active) {
#ifdef _WIN32
        CloseHandle(impl->thread);
#else
        pthread_detach(impl->thread);
#endif
        impl->active = false;
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

unsigned int SE_Thread::getCurrentThreadId() {
#ifdef _WIN32
    return static_cast<unsigned int>((uintptr_t)GetCurrentThread());
#else
    return 0;
#endif
}

struct SE_Mutex::Impl {
#ifdef _WIN32
    CRITICAL_SECTION mtx;
#else
    pthread_mutex_t mtx;
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
#else
        pthread_mutex_init(&impl->mtx, nullptr);
#endif
    }
}

SE_Mutex::~SE_Mutex() {
    if (impl) {
#ifdef _WIN32
        DeleteCriticalSection(&impl->mtx);
#else
        pthread_mutex_destroy(&impl->mtx);
#endif
        delete impl;
        impl = nullptr;
    }
}

void SE_Mutex::lock() {
#ifdef _WIN32
    EnterCriticalSection(&impl->mtx);
#else
    pthread_mutex_lock(&impl->mtx);
#endif
}

void SE_Mutex::unlock() {
#ifdef _WIN32
    LeaveCriticalSection(&impl->mtx);
#else
    pthread_mutex_unlock(&impl->mtx);
#endif
}

bool SE_Mutex::tryLock() {
#ifdef _WIN32
    return TryEnterCriticalSection(&impl->mtx) != 0;
#else
    return pthread_mutex_trylock(&impl->mtx) == 0;
#endif
}
