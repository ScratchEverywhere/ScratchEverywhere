#include <SDL2/SDL.h>
#include <thread.hpp>

// just using SDL2 threads/mutexes for now

struct SE_Thread::Impl {
    SDL_Thread *thread;
};

struct SDL_ThreadData {
    void (*entryPoint)(void *);
    void *args;
};

static int SDL_ThreadWrapper(void *data) {
    SDL_ThreadData *ctx = static_cast<SDL_ThreadData *>(data);

    ctx->entryPoint(ctx->args);

    delete ctx;
    return 0;
}

SE_Thread::SE_Thread() : impl(nullptr) {}

bool SE_Thread::create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio, int coreID, const std::string &name) {
    impl = new Impl;

    SDL_ThreadData *ctx = new SDL_ThreadData;
    ctx->entryPoint = entryPoint;
    ctx->args = args;

    impl->thread = SDL_CreateThreadWithStackSize(SDL_ThreadWrapper, name.c_str(), stackSize, ctx);

    if (impl->thread == nullptr) {
        delete ctx;
        delete impl;
        impl = nullptr;
        return false;
    }

    return true;
}

SE_Thread::~SE_Thread() {
    join();
}

void SE_Thread::join() {
    if (impl != nullptr && impl->thread != nullptr) {
        SDL_WaitThread(impl->thread, nullptr);
        delete impl;
        impl = nullptr;
    }
}

void SE_Thread::detach() {
    if (impl != nullptr && impl->thread != nullptr) {
        SDL_DetachThread(impl->thread);
        impl->thread = nullptr;
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
    SDL_Delay(milliseconds);
}

unsigned int SE_Thread::getCurrentThreadId() {
    return static_cast<unsigned int>(SDL_ThreadID());
}

struct SE_Mutex::Impl {
    SDL_mutex *mtx;
};

SE_Mutex::SE_Mutex() {
    init();
}

void SE_Mutex::init() {
    if (!impl) {
        impl = new Impl;
        impl->mtx = SDL_CreateMutex();
    }
}

SE_Mutex::~SE_Mutex() {
    unlock();
    SDL_DestroyMutex(impl->mtx);
    delete impl;
}

void SE_Mutex::lock() {
    SDL_LockMutex(impl->mtx);
}

void SE_Mutex::unlock() {
    SDL_UnlockMutex(impl->mtx);
}

bool SE_Mutex::tryLock() {
    return SDL_TryLockMutex(impl->mtx) == 0;
}