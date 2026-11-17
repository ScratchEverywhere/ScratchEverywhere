#pragma once

#include <string>
#include <vector>

// Minimal client for the Maliit on-screen keyboard used by Ubuntu Touch.
// SDL has no way to raise the Ubuntu Touch keyboard, so we talk to Maliit
// directly over its private D-Bus socket.
class UbuntuTouchKeyboard {
  public:
    struct Event {
        enum class Type {
            CommitString, // insert `text`, after removing `replaceLength` chars
            Backspace, // remove one character
            Enter, // finish text entry
            Escape, // cancel text entry
            Preedit, // `text` is the current preedit (display only)
            Dismissed, // the input method asked to be hidden
        };

        Type type;
        std::string text;
        int replaceStart = 0;
        int replaceLength = 0;
    };

    UbuntuTouchKeyboard();
    ~UbuntuTouchKeyboard();

    UbuntuTouchKeyboard(const UbuntuTouchKeyboard &) = delete;
    UbuntuTouchKeyboard &operator=(const UbuntuTouchKeyboard &) = delete;

    bool isAvailable() const;

    const std::string &preeditText() const;

    void show();
    void hide();

    // drain pending dbus traffic
    void poll();

    // consume events that arrived since the last call
    std::vector<Event> takeEvents();

  private:
    struct Impl;
    Impl *impl = nullptr;
};
