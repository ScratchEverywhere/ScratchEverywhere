#include "menu.hpp"
#include <render.hpp>

Menu::~Menu() {}

void Menu::render() {}
void Menu::onResize() {}

float Menu::getScrollOffset(Timer deltaTime, const std::optional<Clay_BoundingBox> selectedBoundingBox) {
    const int windowWidth = Render::getWidth();
    const int windowHeight = Render::getHeight();

    const uint16_t padding = 15 * menuManager->scale;

    const Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(CLAY_ID("main"));
    float maxScrollPosition = scrollContainerData.contentDimensions.height - scrollContainerData.scrollContainerDimensions.height;
    if (maxScrollPosition < 0) maxScrollPosition = 0;

    scrollOffset += Input::scrollDelta[1] * deltaTime.getTimeMs();
    if (dragging) {
        if (Input::mousePointer.isPressed) {
            scrollOffset -= Input::mousePointer.y - lastDragPosition[1];
            lastDragPosition = {static_cast<float>(Input::mousePointer.x), static_cast<float>(Input::mousePointer.y)};
        } else {
            dragging = false;
        }
    } else if (Input::mousePointer.isPressed) {
        dragging = true;
        lastDragPosition = {static_cast<float>(Input::mousePointer.x), static_cast<float>(Input::mousePointer.y)};
    }

    if (selectedBoundingBox.has_value()) {
        const Clay_BoundingBox box = selectedBoundingBox.value();
        if (box.y < 0) {
            scrollOffset += (box.y * -1) + padding;
        } else if (box.y + box.height > windowHeight) {
            scrollOffset -= (box.y - windowHeight) + box.height + padding;
        }
    }

    if (scrollOffset > 0) scrollOffset = 0;
    if (scrollOffset < -maxScrollPosition) scrollOffset = -maxScrollPosition;

    return scrollOffset;
}
