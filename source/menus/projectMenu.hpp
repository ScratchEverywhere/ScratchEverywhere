#include "menuManager.hpp"

class SE_EXPORT ProjectMenu : public Menu {
  private:
  public:
    ProjectMenu();
    ~ProjectMenu() override;
    void build(UI::Builder &b) override;
};