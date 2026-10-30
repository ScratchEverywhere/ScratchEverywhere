#if defined(RENDERER_SDL3)
#include <SDL3/SDL.h>
#elif defined(RENDERER_SDL2)
#include <SDL.h>
#elif defined(RENDERER_SDL1)
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#endif
#include <thread.hpp>

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

#if defined(RENDERER_SDL3)
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)SDL_ThreadWrapper);
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, ctx);
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, name.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, (Sint64)stackSize);
    impl->thread = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
#elif defined(RENDERER_SDL2)
    impl->thread = SDL_CreateThreadWithStackSize(SDL_ThreadWrapper, name.c_str(), stackSize, ctx);
#elif defined(RENDERER_SDL1)
    // SDL1 does not support custom stack sizes or thread naming
    (void)stackSize;
    (void)name;
    impl->thread = SDL_CreateThread(SDL_ThreadWrapper, ctx);
#endif

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
#if defined(RENDERER_SDL3) || defined(RENDERER_SDL2)
        SDL_DetachThread(impl->thread);
        impl->thread = nullptr;
#elif defined(RENDERER_SDL1)
        // SDL1 has no detach - fall back to a blocking wait to avoid leaking the thread handle
        SDL_WaitThread(impl->thread, nullptr);
        impl->thread = nullptr;
#endif
    }
}

void SE_Thread::sleep(uint16_t milliseconds) {
#if defined(RENDERER_SDL3)
    SDL_DelayNS((Uint64)milliseconds * SDL_NS_PER_MS);
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL1)
    SDL_Delay((Uint32)milliseconds);
#endif
}

unsigned int SE_Thread::getCurrentThreadId() {
#if defined(RENDERER_SDL3)
    return static_cast<unsigned int>(SDL_GetCurrentThreadID());
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL1)
    return static_cast<unsigned int>(SDL_ThreadID());
#endif
}

struct SE_Mutex::Impl {
#if defined(RENDERER_SDL3) // this is such a minor change whyyyyyyyy
    SDL_Mutex *mtx;
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL1)
    SDL_mutex *mtx;
#endif
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
#if defined(RENDERER_SDL1)
    SDL_mutexP(impl->mtx);
#else
    SDL_LockMutex(impl->mtx);
#endif
}

void SE_Mutex::unlock() {
#if defined(RENDERER_SDL1)
    SDL_mutexV(impl->mtx);
#else
    SDL_UnlockMutex(impl->mtx);
#endif
}

bool SE_Mutex::tryLock() {
#if defined(RENDERER_SDL3)
    return SDL_TryLockMutex(impl->mtx);
#elif defined(RENDERER_SDL2)
    return SDL_TryLockMutex(impl->mtx) == 0;
#elif defined(RENDERER_SDL1)
    // SDL1 has no try-lock
    return false;
#endif
}
