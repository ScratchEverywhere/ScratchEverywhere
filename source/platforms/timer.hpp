#pragma once
#include <cstdint>

class Timer {
  private:
    uint64_t startTime;

  public:
    Timer(const bool autoStart = true);
    /**
     * Starts the clock.
     */
    void start();

    /**
     * Gets the amount of time passed in milliseconds.
     * @return time passed (in ms)
     */
    int getTimeMs();

    double getTimeMsDouble();

    /**
     * Checks if enough time, in milliseconds, has passed since the timer started.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsed(int ms) {
        return getTimeMs() >= ms;
    }

    /**
     * Checks if enough time, in milliseconds, has passed since the timer started, and automatically restarts if true.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsedAndRestart(int ms) {
        if (hasElapsed(ms)) {
            start();
            return true;
        }
        return false;
    }
};