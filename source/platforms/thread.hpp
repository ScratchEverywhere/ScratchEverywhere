#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class SE_Thread {
  public:
    SE_Thread();
    virtual ~SE_Thread();

    SE_Thread(const SE_Thread &) = delete;
    SE_Thread &operator=(const SE_Thread &) = delete;

    /**
     * @param entryPoint a void function that the thread will enter at.
     * @param args any arguments you wanna pass to the function.
     * @param stackSize the stack size of the thread.
     * @param prio [3DS] The priority of the thread. Higher is more priority, lower is less.
     * @param coreID [3DS] The CPU core the thread should run on. -2 chooses the default core, -1 chooses every core, 0 is the main core, and 1 and is the system core.
     * @param name The name to give the thread. Only used on some platforms.
     */
    bool create(void (*entryPoint)(void *), void *args, size_t stackSize, int prio = 1, int coreID = -2, const std::string &name = "");
    void join();
    void detach();

    static void sleep(uint16_t milliseconds);
    static unsigned int getCurrentThreadId();

  private:
    struct Impl;
    Impl *impl;
};

class SE_Mutex {
  public:
    SE_Mutex();
    ~SE_Mutex();

    SE_Mutex(const SE_Mutex &) = delete;
    SE_Mutex &operator=(const SE_Mutex &) = delete;

    void init();
    void lock();
    void unlock();
    bool tryLock();

  private:
    struct Impl;
    Impl *impl;
};