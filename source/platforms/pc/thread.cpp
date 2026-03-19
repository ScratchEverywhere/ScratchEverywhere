#include <chrono>
#include <mutex>
#include <thread.hpp>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

struct SE_Thread::Impl {
    std::thread thread;
};

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    impl = new Impl;
    impl->thread = std::thread(entryPoint, args);
    return true;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr && impl->thread.joinable()) {
        impl->thread.join();
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl->thread.joinable()) {
        impl->thread.detach();
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

unsigned int SE_Thread::getCurrentThreadId() {
    return static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
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