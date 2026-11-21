#pragma once
#include "menuManager.hpp"

class SE_EXPORT Keyboard {
  private:
    static bool capitalized;
    static bool opened;
    static void letter(UI::Builder &b, const std::string &lowercase, const std::string &uppercase);

  public:
    static void open(UI::Builder &b);
    static bool isOpen();
    static void close(UI::Builder &b);

    static void build(UI::Builder &b);
};