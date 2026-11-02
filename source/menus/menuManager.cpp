#include "menuManager.hpp"
#include "renderer.hpp"
#include "ui.hpp"

Menu *MenuManager::currentMenu = nullptr;
UI::Builder MenuManager::builder = {};
UI::Pool MenuManager::elements = {};

void MenuManager::render() {
    if (!currentMenu) return;
    builder.begin(elements);
    currentMenu->build(builder);
    builder.end();
    builder.layout();

    Render::beginFrame(1, 255, 255, 255);

    UI::Painter::render(elements);

    Render::endFrame(false);
}