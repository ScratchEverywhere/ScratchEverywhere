#include <algorithm>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <thread.hpp>
#include <xsdk/thread.h>
struct SE_Thread::Impl {
    pthread_t thread;
    bool active = false;
};
struct Entry {
    void (*function)(void *);
    void *argument;
};
SE_Thread::SE_Thread() : impl(nullptr) {}
SE_Thread::~SE_Thread() {
    if (impl) {
        join();
        delete impl;
    }
}
bool SE_Thread::create(void (*function)(void *), void *argument, size_t stackSize, int, int, const std::string &) {
    if (impl) return false;
    auto state = std::make_unique<Impl>();
    auto entry = std::make_unique<Entry>(Entry{function, argument});
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) return false;
    if (stackSize) {
        const size_t clampedStack = std::max<size_t>(stackSize, 128 * 1024);
        if (pthread_attr_setstacksize(&attr, clampedStack) != 0) {
            pthread_attr_destroy(&attr);
            return false;
        }
    }
    int result = pthread_create(&state->thread, &attr, [](void *p) -> void * {std::unique_ptr<Entry> e(static_cast<Entry *>(p));e->function(e->argument);return nullptr; }, entry.get());
    pthread_attr_destroy(&attr);
    if (result) return false;
    entry.release();
    state->active = true;
    impl = state.release();
    return true;
}
void SE_Thread::join() {
    if (impl) {
        if (impl->active) {
            pthread_join(impl->thread, nullptr);
            impl->active = false;
        }
        delete impl;
        impl = nullptr;
    }
}
void SE_Thread::detach() {
    if (impl) {
        if (impl->active) {
            pthread_detach(impl->thread);
            impl->active = false;
        }
        delete impl;
        impl = nullptr;
    }
}
void SE_Thread::sleep(uint16_t ms) { xsdk_thread_sleep(ms); }
unsigned SE_Thread::getCurrentThreadId() { return static_cast<unsigned>(reinterpret_cast<uintptr_t>(pthread_self())); }
struct SE_Mutex::Impl {
    std::mutex mutex;
};
SE_Mutex::SE_Mutex() : impl(nullptr) { init(); }
SE_Mutex::~SE_Mutex() { delete impl; }
void SE_Mutex::init() {
    if (!impl) impl = new Impl;
}
void SE_Mutex::lock() { impl->mutex.lock(); }
void SE_Mutex::unlock() { impl->mutex.unlock(); }
bool SE_Mutex::tryLock() { return impl->mutex.try_lock(); }
