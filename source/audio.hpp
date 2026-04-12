#pragma once

class SoundPlayer {
  public:
    static void init();
    static void cleanupAudio();
    static void deinit();
    static void flushAudio();
};
