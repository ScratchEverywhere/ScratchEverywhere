#pragma once
#include <string>
#include <utility>

class SharedString {
  private:
    struct Control {
        std::string str;
        long refcount;
        explicit Control(std::string s) : str(std::move(s)), refcount(1) {}
    };

    Control *ctrl;

    void release() {
        if (ctrl && --ctrl->refcount == 0) delete ctrl;
    }

  public:
    SharedString() : ctrl(new Control(std::string())) {}
    explicit SharedString(std::string s) : ctrl(new Control(std::move(s))) {}

    SharedString(const SharedString &other) noexcept : ctrl(other.ctrl) {
        if (ctrl) ++ctrl->refcount;
    }

    SharedString(SharedString &&other) noexcept : ctrl(other.ctrl) {
        other.ctrl = nullptr;
    }

    SharedString &operator=(const SharedString &other) noexcept {
        if (this != &other) {
            release();
            ctrl = other.ctrl;
            if (ctrl) ++ctrl->refcount;
        }
        return *this;
    }

    SharedString &operator=(SharedString &&other) noexcept {
        if (this != &other) {
            release();
            ctrl = other.ctrl;
            other.ctrl = nullptr;
        }
        return *this;
    }

    ~SharedString() { release(); }

    const std::string &operator*() const { return ctrl->str; }
    const std::string *get() const { return ctrl ? &ctrl->str : nullptr; }
};
