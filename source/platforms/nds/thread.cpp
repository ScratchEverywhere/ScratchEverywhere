#include <thread.hpp>

// stubbed for now :nategrin:

struct SE_Thread::Impl {};

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) { return false; }

SE_Thread::~SE_Thread() {}

void SE_Thread::join() {}

void SE_Thread::detach() {}

void SE_Thread::sleep(uint16_t milliseconds) {}

unsigned int SE_Thread::getCurrentThreadId() {}

struct SE_Mutex::Impl {};

SE_Mutex::SE_Mutex() {}

void SE_Mutex::init() {}

SE_Mutex::~SE_Mutex() {}

void SE_Mutex::lock() {}

void SE_Mutex::unlock() {}

bool SE_Mutex::tryLock() {}
