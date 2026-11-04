#include "menuManager.hpp"

class SE_EXPORT SettingsMenu : public Menu {
  private:
  public:
    SettingsMenu();
    ~SettingsMenu() override;
    void build(UI::Builder &b) override;
};