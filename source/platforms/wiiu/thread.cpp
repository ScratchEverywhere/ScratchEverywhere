#include <chrono>
#include <mutex>
#include <thread.hpp>
#include <thread>

// std::thread and std::mutex seem to work fine on Wii U

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
    std::mutex mtx;
};

SE_Mutex::SE_Mutex() {
    init();
}

void SE_Mutex::init() {
    if (!impl) impl = new Impl;
}

SE_Mutex::~SE_Mutex() {
    delete impl;
}

void SE_Mutex::lock() {
    impl->mtx.lock();
}

void SE_Mutex::unlock() {
    impl->mtx.unlock();
}

bool SE_Mutex::tryLock() {
    return impl->mtx.try_lock();
}