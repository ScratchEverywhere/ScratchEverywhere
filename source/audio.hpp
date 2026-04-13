#pragma once

class SoundPlayer {
  public:
    static bool init();
    static void cleanupAudio();
    static void deinit();
    static void flushAudio();
};
