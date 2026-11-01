#include "menuManager.hpp"

class SE_EXPORT MainMenu : public Menu {
  public:
    MainMenu();
    ~MainMenu() override;
    void build(UI::Builder &b) override;
};