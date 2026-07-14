#pragma once

#include "image.hpp"
#include "menu.hpp"
#include "translation.hpp"
#include <memory>
#include <vector>

class LanguageMenu : public Menu {
  private:
    std::string title;
    std::shared_ptr<Image> indicator;
    int selected = -1;

    std::vector<TranslationManager::LanguageInfo> langs;
    std::vector<Clay_String> clayLangNames;

  public:
    LanguageMenu(void *userdata);
    ~LanguageMenu();
    void render() override;
};
