#include "menuManager.hpp"

Menu *MenuManager::currentMenu = nullptr;
UI::Builder MenuManager::builder = {};
UI::Pool MenuManager::elements = {};

void MenuManager::render() {
    if (!currentMenu) return;
    builder.begin(elements);
    currentMenu->build(builder);
    builder.end();
}