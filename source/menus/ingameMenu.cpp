#include "ingameMenu.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <log.hpp>
#include <runtime.hpp>

namespace {

constexpr uint8_t kFont = 1;

// TODO: increase or something
constexpr size_t kMaxListRows = 50;

// TODO: mopve to styles
constexpr UI::Color kBorder{194, 204, 217, 255};
constexpr UI::Color kPanel{229, 240, 255, 255};
constexpr UI::Color kWhite{255, 255, 255, 255};
constexpr UI::Color kBlack{0, 0, 0, 255};
constexpr UI::Color kListRow{252, 102, 44, 255};
constexpr UI::Color kTrack{178, 178, 178, 255};
constexpr UI::Color kTrackFill{239, 239, 239, 255};
constexpr UI::Color kHandle{0, 115, 252, 255};

uint8_t px(float v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(v)), 0, 255)); }
int16_t pos(float v) { return static_cast<int16_t>(std::lround(v)); }
UI::Size fixedPx(float v) { return UI::Size::fixed(static_cast<uint16_t>(std::max(0.0f, std::round(v)))); }
uint16_t scrollId(const std::string &id) { return 0x8000 | (std::hash<std::string>{}(id) & 0x7FFF); }

template <class C>
UI::Color toColor(const C &c) {
    return {static_cast<uint8_t>(c.r), static_cast<uint8_t>(c.g), static_cast<uint8_t>(c.b), 255};
}

void label(UI::Builder &b, const std::string &s, float fontScale, UI::Color color,
           UI::AlignMode align = UI::AlignMode::Left) {
    b.text({.text = s, .fontId = kFont, .scale = fontScale, .color = color, .align = align});
}

void minWidth(UI::Builder &b, float w) {
    b.element({.w = fixedPx(w), .h = UI::Size::fixed(0)});
}

void valuePill(UI::Builder &b, UI::Color bg, const std::string &value, float fontScale, float padX, float minW) {
    b.element({.padding = UI::Padding::axis(px(padX), 0), .align = UI::AlignMode::Center, .color = bg}, [&] {
        minWidth(b, minW);
        label(b, value, fontScale, kWhite);
    });
}

struct Inner {
    UI::ChildDir dir = UI::ChildDir::UpDown;
    uint8_t gap = 0;
    UI::Padding pad = {};
    UI::AlignMode align = UI::AlignMode::Left;
};

template <class F>
void frame(UI::Builder &b, float scale, float x, float y, UI::Size w, UI::Size h,
           UI::Color fill, const Inner &in, F &&body) {
    const uint8_t border = std::max<uint8_t>(1, px(scale));
    const bool sized = w.mode != UI::SizeMode::Fit;

    b.element({.flags = UI::Flag::Visible | UI::Flag::Floating,
               .w = w,
               .h = h,
               .x = pos(x),
               .y = pos(y),
               .padding = UI::Padding::all(border),
               .color = kBorder},
              [&] {
                  b.element({.w = sized ? UI::Size::grow() : UI::Size::fit(),
                             .h = sized ? UI::Size::grow() : UI::Size::fit(),
                             .childDirection = in.dir,
                             .childGap = in.gap,
                             .padding = in.pad,
                             .align = in.align,
                             .color = fill},
                            body);
              });
}

float gutterWidth(size_t count, float fontScale, float scale) {
    auto probe = createTextObject(std::to_string(count), 0, 0, UI::fontPaths[kFont]);
    probe->setScale(fontScale);
    return std::max(20.0f * scale, probe->getSize()[0] + 10.0f * scale);
}

} // namespace

IngameMenu::IngameMenu() {
}

IngameMenu::~IngameMenu() {
}

void IngameMenu::variableNormal(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));

    frame(b, scale, x, y, UI::Size::fit(), UI::Size::fit(), kPanel,
          Inner{UI::ChildDir::LeftRight, px(4 * scale), UI::Padding::axis(px(4 * scale), px(2 * scale)), UI::AlignMode::Center},
          [&] {
              label(b, var.displayName, font, kBlack);
              valuePill(b, valueColor, Render::getVariableValueString(var.value), font, 4 * scale, 32 * scale);
          });
}

void IngameMenu::variableBig(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));

    frame(b, scale, x, y, UI::Size::fit(), UI::Size::fit(), valueColor,
          Inner{UI::ChildDir::UpDown, 0, UI::Padding::axis(px(2 * scale), 0), UI::AlignMode::Center},
          [&] {
              minWidth(b, 36 * scale);
              label(b, Render::getVariableValueString(var.value), 1.25f * font, kWhite);
          });
}

void IngameMenu::variableSlider(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));

    const double lo = var.sliderMin, hi = var.sliderMax;
    const double value = var.value.get<double>();
    const double n = hi > lo ? std::clamp((value - lo) / (hi - lo), 0.0, 1.0) : 0.0;
    const int before = std::clamp(static_cast<int>(std::lround(n * 1000.0)), 1, 999);

    frame(b, scale, x, y, UI::Size::fit(), UI::Size::fit(), kPanel,
          Inner{UI::ChildDir::UpDown, px(4 * scale), UI::Padding::axis(px(4 * scale), px(2 * scale)), UI::AlignMode::Left},
          [&] {
              b.element({.childDirection = UI::ChildDir::LeftRight, .childGap = px(4 * scale), .align = UI::AlignMode::Center}, [&] {
                  label(b, var.displayName, font, kBlack);
                  valuePill(b, valueColor, Render::getVariableValueString(var.value), font, 4 * scale, 32 * scale);
              });

              b.element({.w = UI::Size::grow(), .padding = UI::Padding::all(px(scale)), .color = kTrack}, [&] {
                  b.element({.w = UI::Size::grow(),
                             .h = fixedPx(7 * scale),
                             .childDirection = UI::ChildDir::LeftRight,
                             .align = UI::AlignMode::Center,
                             .color = kTrackFill},
                            [&] {
                                b.element({.w = UI::Size::make(UI::SizeMode::Grow, before)});
                                b.element({.w = fixedPx(13 * scale), .h = fixedPx(13 * scale), .color = kHandle});
                                b.element({.w = UI::Size::make(UI::SizeMode::Grow, 1000 - before)});
                            });
              });
          });
}

void IngameMenu::listVariable(UI::Builder &b, const Value &item, size_t index, float gutterW) {
    const float font = scale / 2.0f;

    b.element({
                  .flags = UI::Flag::Visible | UI::Flag::Focusable,
                  .w = UI::Size::grow(),
                  .childDirection = UI::ChildDir::LeftRight,
                  .align = UI::AlignMode::Center,
                  .onClick = [&] {
                      Input::setClipboardText(Render::getListValueString(item));
                  },
              },
              [&] {
                  b.element({.w = fixedPx(gutterW), .align = UI::AlignMode::Center}, [&] {
                      label(b, std::to_string(index + 1), font, kBlack);
                  });
                  b.element({
                                .w = UI::Size::grow(),
                                .padding = UI::Padding::axis(px(4 * scale), px(2 * scale)),
                                .color = kListRow,
                            },
                            [&] {
                                label(b, Render::getListValueString(item), font, kWhite);
                            });
              });
}

void IngameMenu::list(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const float w = var.width * scale;
    const float h = var.height * scale;
    const uint8_t gap = px(3 * scale);
    const float gutter = gutterWidth(var.list.size(), font, scale);
    const size_t rows = std::min(var.list.size(), kMaxListRows);

    frame(b, scale, x - scale, y - scale, fixedPx(w + 2 * scale), fixedPx(h + 2 * scale), kPanel, Inner{}, [&] {
        b.element({.w = UI::Size::grow(),
                   .padding = UI::Padding::axis(0, px(2 * scale)),
                   .align = UI::AlignMode::Center,
                   .color = kWhite},
                  [&] {
                      label(b, var.displayName, font, kBlack, UI::AlignMode::Center);
                  });

        b.element({.flags = UI::Flag::Visible | UI::Flag::Clip | UI::Flag::ScrollY,
                   .w = UI::Size::grow(),
                   .h = UI::Size::grow(),
                   .childGap = gap,
                   .padding = {0, px(4 * scale), gap, gap},
                   .id = scrollId(var.id)},
                  [&] {
                      if (var.list.empty()) {
                          label(b, "(empty)", font, kBlack, UI::AlignMode::Center);
                          return;
                      }
                      for (size_t i = 0; i < rows; ++i)
                          listVariable(b, var.list[i], i, gutter);
                  });

        b.element({.w = UI::Size::grow(),
                   .padding = UI::Padding::axis(0, px(2 * scale)),
                   .align = UI::AlignMode::Center,
                   .color = kWhite},
                  [&] {
                      label(b, "length " + std::to_string(var.list.size()), font, kBlack, UI::AlignMode::Center);
                  });
    });
}

void IngameMenu::build(UI::Builder &b) {
    scale = Render::renderScale;
    const float screenW = Render::getWidth();
    const float screenH = Render::getHeight();

    const float screenAspect = screenW / screenH;
    const float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barX = 0.0f, barY = 0.0f;
    if (screenAspect > projectAspect) barX = (screenW - Scratch::projectWidth * scale) / 2.0f;
    else if (screenAspect < projectAspect) barY = (screenH - Scratch::projectHeight * scale) / 2.0f;

    for (auto &[id, var] : Render::monitors) {
        if (!var.visible) continue;

        // Turbowarp's monitor position math
        const float projectX = (var.x + offsetX) + (Scratch::projectWidth - 480) * 0.5f;
        const float projectY = (var.y + offsetY) + (Scratch::projectHeight - 360) * 0.5f;
        const float x = projectX * scale + barX;
        const float y = projectY * scale + barY;

        if (var.mode == "list") list(b, var, x + 4 * scale, y + 2 * scale);
        else if (var.mode == "large") variableBig(b, var, x, y);
        else if (var.mode == "slider") variableSlider(b, var, x, y);
        else variableNormal(b, var, x, y);
    }
}