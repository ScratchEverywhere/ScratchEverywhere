#pragma once
#ifdef ENABLE_PROJECTRECEIVER
#include <memory>
#include <string>
#include <vector>

class ProjectReceiver {
  public:
    static bool init();
    static void deinit();
    static void update();
    static std::string getShortCode();

  private:
    static void handleClient(int client_fd);
    static bool isInitialized;
    static int listen_fd;
};
#else
class ProjectReceiver {
  public:
    static bool init() { return false; }
    static void deinit() {}
    static void update() {}
    static std::string getShortCode() { return ""; }
};
#endif
