#include "keyboard.hpp"
#include "theme.hpp"
#include "types.hpp"
#include "ui.hpp"

bool Keyboard::capitalized = false;
bool Keyboard::opened = false;

void Keyboard::open(UI::Builder &b) {
    opened = true;
    b.dirty = true;
}

bool Keyboard::isOpen() {
    return opened;
}

void Keyboard::close(UI::Builder &b) {
    opened = false;
    b.dirty = true;
}

void Keyboard::letter(UI::Builder &b, const std::string &lowercase, const std::string &uppercase) {
    const std::string letter = capitalized ? uppercase : lowercase;

    b.element({
                  .flags = UI::Flag::Focusable,
                  .w = UI::Size::fixed(32),
                  .h = UI::Size::fixed(32),
                  .align = UI::AlignMode::Center,
                  .style = UI::StyleId::KeyboardKey,
                  .onClick = [&b, letter] {
                      Input::inputText += letter;
                      b.dirty = true;
                  },
              },
              [&] {
                  b.text({
                      .text = letter,
                      .scale = 0.75,
                      .color = {76, 76, 76, 255},
                  });
              });
}

template <typename F>
void key(UI::Builder &b, const std::string &name, F &&onClick) {
    b.element({
                  .flags = UI::Flag::Focusable,
                  .w = UI::Size::fit(),
                  .h = UI::Size::fixed(32),
                  .padding = UI::Padding::axis(5, 0),
                  .align = UI::AlignMode::Center,
                  .style = UI::StyleId::KeyboardKey,
                  .onClick = onClick,
              },
              [&] {
                  b.text({
                      .text = name,
                      .scale = 0.75,
                      .color = {76, 76, 76, 255},
                  });
              });
}

template <typename F>
void row(UI::Builder &b, F &&body) {
    b.element({
                  .w = UI::Size::fit(),
                  .h = UI::Size::fit(),
                  .childDirection = UI::ChildDir::LeftRight,
                  .align = UI::AlignMode::Center,
                  .color = {0, 0, 0, 0},
              },
              [&] {
                  body();
              });
}

void Keyboard::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fit(),
                  .childDirection = UI::ChildDir::UpDown,
                  .style = UI::StyleId::KeyboardBackground,
              },
              [&] {
                  row(b, [&b] {
                      letter(b, "1", "!");
                      letter(b, "2", "@");
                      letter(b, "3", "#");
                      letter(b, "4", "$");
                      letter(b, "5", "%");
                      letter(b, "6", "^");
                      letter(b, "7", "&");
                      letter(b, "8", "*");
                      letter(b, "9", "(");
                      letter(b, "0", ")");
                      letter(b, "-", "_");
                      letter(b, "=", "+");
                      key(b, "Backspace", [&] {
                          if (!Input::inputText.empty())
                              Input::inputText.pop_back();
                          b.dirty = true;
                      });
                  });
                  row(b, [&b] {
                      letter(b, "q", "Q");
                      letter(b, "w", "W");
                      letter(b, "e", "E");
                      letter(b, "r", "R");
                      letter(b, "t", "T");
                      letter(b, "y", "Y");
                      letter(b, "u", "U");
                      letter(b, "i", "I");
                      letter(b, "o", "O");
                      letter(b, "p", "P");
                      letter(b, "[", "{");
                      letter(b, "]", "}");
                      letter(b, "\\", "|");
                  });
                  row(b, [&b] {
                      letter(b, "a", "A");
                      letter(b, "s", "S");
                      letter(b, "d", "D");
                      letter(b, "f", "F");
                      letter(b, "g", "G");
                      letter(b, "h", "H");
                      letter(b, "j", "J");
                      letter(b, "k", "K");
                      letter(b, "l", "L");
                      letter(b, ";", ":");
                      letter(b, "'", "\"");
                  });
                  row(b, [&b] {
                      key(b, "Caps Lock", [&] {
                          Keyboard::capitalized = !Keyboard::capitalized;
                          b.dirty = true;
                      });
                      letter(b, "z", "Z");
                      letter(b, "x", "X");
                      letter(b, "c", "C");
                      letter(b, "v", "V");
                      letter(b, "b", "B");
                      letter(b, "n", "N");
                      letter(b, "m", "M");
                      letter(b, ",", "<");
                      letter(b, ".", ">");
                      letter(b, "/", "?");
                      key(b, "Enter", [&] {
                          close(b);
                      });
                  });

                  row(b, [&b] {
                      key(b, "Space", [&] {
                          Input::inputText += " ";
                          b.dirty = true;
                      });
                  });
              });
}