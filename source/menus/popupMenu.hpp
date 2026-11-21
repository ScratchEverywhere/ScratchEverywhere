#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"

enum class PopupType : bool {
    AcceptOrCancel,
    Acknowledgement,
};

enum class PopupState : uint8_t {
    None,
    Accepted,
    Cancelled,
};

class SE_EXPORT PopupMenu : public SettingsMenu {
  private:
    std::string text;

  public:
    PopupType type;
    PopupState state = PopupState::None;

    PopupMenu(PopupType type, const std::string &text);
    ~PopupMenu() override;
    void build(UI::Builder &b) override;
};