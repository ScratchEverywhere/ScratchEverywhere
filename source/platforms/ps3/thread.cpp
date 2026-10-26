#include <thread.hpp>
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

struct SE_Mutex::Impl {};

SE_Mutex::SE_Mutex() : impl(nullptr) {
    init();
}

void SE_Mutex::init() {
    if (!impl) impl = new Impl;
}

SE_Mutex::~SE_Mutex() {
    delete impl;
    impl = nullptr;
}

void SE_Mutex::lock() {
}

void SE_Mutex::unlock() {
}

bool SE_Mutex::tryLock() {
    return true;
}