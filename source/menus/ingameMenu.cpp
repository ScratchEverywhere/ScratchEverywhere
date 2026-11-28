#include "ingameMenu.hpp"
#include "menuManager.hpp"
#include "theme.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <log.hpp>
#include <runtime.hpp>

namespace {

constexpr UI::Color kWhite{255, 255, 255, 255};
constexpr UI::Color kBlack{0, 0, 0, 255};

uint8_t px(float v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(v)), 0, 255)); }
int16_t pos(float v) { return static_cast<int16_t>(std::lround(v)); }
UI::Size fixedPx(float v) { return UI::Size::fixed(static_cast<uint16_t>(std::max(0.0f, std::round(v)))); }
uint16_t scrollId(const std::string &id) { return 0x8000 | (std::hash<std::string>{}(id) & 0x7FFF); }

template <class C>
UI::Color toColor(const C &c) {
    return {static_cast<uint8_t>(c.r), static_cast<uint8_t>(c.g), static_cast<uint8_t>(c.b), 255};
}

void labelBold(UI::Builder &b, const std::string &s, float fontScale, UI::Color color,
               UI::AlignMode align = UI::AlignMode::Left) {
    b.text({.text = s, .fontId = 0, .scale = fontScale, .color = color, .align = align});
}

void label(UI::Builder &b, const std::string &s, float fontScale, UI::Color color,
           UI::AlignMode align = UI::AlignMode::Left) {
    b.text({.text = s, .fontId = 1, .scale = fontScale, .color = color, .align = align});
}

void minWidth(UI::Builder &b, float w) {
    b.element({.w = fixedPx(w), .h = UI::Size::fixed(0)});
}

void valuePill(UI::Builder &b, float scale, UI::Color bg, const std::string &value, float fontScale, float padX, float minW) {
    b.element({
                  .padding = UI::Padding::axis(px(padX), 0),
                  .align = UI::AlignMode::Center,
                  .color = bg,
                  .style = UI::StyleId::MonitorValue,
              },
              [&] {
                  minWidth(b, minW);
                  label(b, value, fontScale, kWhite);
              });
}

struct Inner {
    UI::ChildDir dir = UI::ChildDir::UpDown;
    uint8_t gap = 0;
    UI::Padding pad = {};
    UI::AlignMode align = UI::AlignMode::Left;
    UI::Color tint = {};
    uint8_t flags = 0;
};

template <class F>
void panel(UI::Builder &b, float scale, uint8_t style, float x, float y, UI::Size w, UI::Size h,
           const Inner &in, F &&body) {
    b.element({
                  .flags = static_cast<uint8_t>(UI::Flag::Floating | in.flags),
                  .w = w,
                  .h = h,
                  .x = pos(x),
                  .y = pos(y),
                  .childDirection = in.dir,
                  .childGap = in.gap,
                  .padding = in.pad,
                  .align = in.align,
                  .color = in.tint,
                  .style = style,
              },
              body);
}

float gutterWidth(size_t count, float fontScale, float scale) {
    auto probe = createTextObject(std::to_string(count), 0, 0, UI::Theme::fonts[0]);
    probe->setScale(fontScale);
    return std::max(20.0f * scale, probe->getSize()[0] + 10.0f * scale);
}

uint16_t sliderId(const std::string &id) { return 0x4000 | (std::hash<std::string>{}(id) & 0x3FFF); }

void setSliderValue(const std::string &monitorId, double t) {
    const auto it = Render::monitors.find(monitorId);
    if (it == Render::monitors.end()) return;
    auto &var = it->second;

    double v = var.sliderMin + t * (var.sliderMax - var.sliderMin);
    if (var.isDiscrete) v = static_cast<int>(v);
    else v = std::round(v * 100.0) / 100.0;

    var.value = Value(v);
    if (var.opcode == "data_variable") {
        for (auto &spr : Scratch::sprites)
            if (spr->variables.find(var.id) != spr->variables.end())
                BlockExecutor::setVariableValue(var.id, Value(v), spr);
    }
}

void pageButton(UI::Builder &b, float scale, const char *glyph, bool enabled, std::function<void()> onClick) {
    const UI::Size size = fixedPx(12 * scale);
    if (!enabled) {
        b.element({.w = size, .h = size});
        return;
    }
    b.element({.flags = UI::Flag::Focusable,
               .w = size,
               .h = size,
               .align = UI::AlignMode::Center,
               .style = UI::StyleId::PageButton,
               .onClick = std::move(onClick)},
              [&] {
                  labelBold(b, glyph, scale / 2.0f, kBlack, UI::AlignMode::Center);
              });
}

} // namespace

IngameMenu::IngameMenu() {
    MenuManager::canChangeTabs = false;
}

IngameMenu::~IngameMenu() {
}

void IngameMenu::variableNormal(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));
    x += scale;
    y += scale;

    panel(b, scale, UI::StyleId::MonitorPanel, x, y, UI::Size::fit(), UI::Size::fit(),
          Inner{UI::ChildDir::LeftRight, px(4 * scale), UI::Padding::axis(px(4 * scale), px(2 * scale)), UI::AlignMode::Center},
          [&] {
              labelBold(b, var.displayName, font, kBlack);
              valuePill(b, scale, valueColor, Render::getVariableValueString(var.value), font, 4 * scale, 32 * scale);
          });
}

void IngameMenu::variableBig(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));
    x += scale;
    y += scale;

    panel(b, scale, UI::StyleId::MonitorLarge, x, y, UI::Size::fit(), UI::Size::fit(),
          Inner{UI::ChildDir::UpDown, 0, UI::Padding::axis(px(2 * scale), 0), UI::AlignMode::Center, valueColor},
          [&] {
              minWidth(b, 36 * scale);
              label(b, Render::getVariableValueString(var.value), 1.25f * font, kWhite);
          });
}

void IngameMenu::variableSlider(UI::Builder &b, Monitor &var, float x, float y) {
    const float font = scale / 2.0f;
    const auto valueColor = toColor(Render::getMonitorValueColor(var.opcode));
    x += scale;
    y += scale;

    const double lo = var.sliderMin, hi = var.sliderMax;
    const double value = var.value.get<double>();
    const double n = hi > lo ? std::clamp((value - lo) / (hi - lo), 0.0, 1.0) : 0.0;
    const int before = std::clamp(static_cast<int>(std::lround(n * 1000.0)), 1, 999);

    panel(b, scale, UI::StyleId::MonitorPanel, x, y, UI::Size::fit(), UI::Size::fit(),
          Inner{UI::ChildDir::UpDown, px(4 * scale), UI::Padding::axis(px(4 * scale), px(2 * scale)), UI::AlignMode::Left},
          [&] {
              b.element({.childDirection = UI::ChildDir::LeftRight, .childGap = px(4 * scale), .align = UI::AlignMode::Center}, [&] {
                  labelBold(b, var.displayName, font, kBlack);
                  valuePill(b, scale, valueColor, Render::getVariableValueString(var.value), font, 4 * scale, 32 * scale);
              });

              const float handleW = 13 * scale;
              const float handlePx = std::round(handleW) * b.scale;

              b.element({.w = UI::Size::grow(),
                         .padding = UI::Padding::axis(0, px(8 * scale)),
                         .id = sliderId(var.id),
                         .onDrag = [monitorId = var.id, handlePx](const UI::DragPoint &p) {
                             const float travel = std::max(1.0f, p.w - handlePx);
                             const double t = std::clamp((p.x - handlePx / 2.0f) / travel, 0.0f, 1.0f);
                             setSliderValue(monitorId, t);
                         }},
                        [&] {
                            b.element({
                                          .w = UI::Size::grow(),
                                          .h = fixedPx(7 * scale),
                                          .childDirection = UI::ChildDir::LeftRight,
                                          .align = UI::AlignMode::Center,
                                          .style = UI::StyleId::MonitorSliderTrack,
                                      },
                                      [&] {
                                          b.element({.w = UI::Size::make(UI::SizeMode::Grow, before)});
                                          b.element({
                                              .w = fixedPx(13 * scale),
                                              .h = fixedPx(13 * scale),
                                              .style = UI::StyleId::MonitorSliderHandle,
                                          });
                                          b.element({.w = UI::Size::make(UI::SizeMode::Grow, 1000 - before)});
                                      });
                        });
          });
}

void IngameMenu::listVariable(UI::Builder &b, const Value &item, size_t index, float gutterW) {
    const float font = scale / 2.0f;
    const std::string value = Render::getListValueString(item);

    b.element({.w = UI::Size::grow(),
               .childDirection = UI::ChildDir::LeftRight,
               .align = UI::AlignMode::Center},
              [&] {
                  b.element({.w = fixedPx(gutterW), .align = UI::AlignMode::Center}, [&] {
                      labelBold(b, std::to_string(index + 1), font, kBlack);
                  });

                  b.element({.flags = UI::Flag::Focusable,
                             .w = UI::Size::grow(),
                             .padding = UI::Padding::axis(px(4 * scale), px(2 * scale)),
                             .style = UI::StyleId::MonitorListItem,
                             .onClick = [value] {
                                 Input::setClipboardText(value);
                             }},
                            [&] {
                                label(b, value, font, kWhite);
                            });
              });
}

void IngameMenu::list(UI::Builder &b, Monitor &var, float x, float y) {
    const size_t maxListRows = 50;
    const float font = scale / 2.0f;
    const float w = var.width * scale;
    const float h = var.height * scale;
    const uint8_t gap = px(3 * scale);
    const float gutter = gutterWidth(var.list.size(), font, scale);

    const size_t pages = std::max<size_t>(1, (var.list.size() + maxListRows - 1) / maxListRows);
    size_t &page = listPages[var.id];
    page = std::min(page, pages - 1);
    const size_t first = page * maxListRows;
    const size_t last = std::min(var.list.size(), first + maxListRows);

    auto changePage = [this, &b, id = var.id](int delta) {
        size_t &p = listPages[id];
        p = delta < 0 ? (p > 0 ? p - 1 : 0) : p + 1;
        if (UI::ScrollState *s = MenuManager::elements->findOrCreateScroll(scrollId(id))) s->y = 0;
        b.dirty = true;
    };

    panel(b, scale, UI::StyleId::MonitorPanel, x, y, fixedPx(w), fixedPx(h), Inner{}, [&] {
        b.element({
                      .w = UI::Size::grow(),
                      .padding = UI::Padding::axis(0, px(2 * scale)),
                      .align = UI::AlignMode::Center,
                      .style = UI::StyleId::MonitorListBar,
                  },
                  [&] {
                      labelBold(b, var.displayName, font, kBlack, UI::AlignMode::Center);
                  });

        b.element({.flags = UI::Flag::ScrollY,
                   .w = UI::Size::grow(),
                   .h = UI::Size::grow(),
                   .childGap = gap,
                   .padding = {0, px(4 * scale), gap, gap},
                   .id = scrollId(var.id)},
                  [&] {
                      if (var.list.empty()) {
                          labelBold(b, "(empty)", font, kBlack, UI::AlignMode::Center);
                          return;
                      }
                      for (size_t i = first; i < last; ++i)
                          listVariable(b, var.list[i], i, gutter);
                  });

        b.element({
                      .w = UI::Size::grow(),
                      .childDirection = UI::ChildDir::LeftRight,
                      .childGap = px(2 * scale),
                      .padding = UI::Padding::axis(px(2 * scale), px(2 * scale)),
                      .align = UI::AlignMode::Center,
                      .style = UI::StyleId::MonitorListBar,
                  },
                  [&] {
                      if (pages > 1) pageButton(b, scale, "<", page > 0, [changePage] { changePage(-1); });

                      std::string text = "length " + std::to_string(var.list.size());
                      if (pages > 1) text += " (" + std::to_string(page + 1) + " / " + std::to_string(pages) + ")";
                      b.element({.w = UI::Size::grow(), .align = UI::AlignMode::Center}, [&] {
                          labelBold(b, text, font, kBlack, UI::AlignMode::Center);
                      });

                      if (pages > 1) pageButton(b, scale, ">", page + 1 < pages, [changePage] { changePage(1); });
                  });
    });
}

void IngameMenu::askMenu(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fit(),
                  .padding = UI::Padding::all(10),
              },
              [&] {
                  b.element({
                                .w = UI::Size::grow(),
                                .h = UI::Size::fit(),
                                .childDirection = UI::ChildDir::LeftRight,
                                .childGap = 5,
                                .padding = UI::Padding::all(15),
                                .align = UI::AlignMode::Center,
                                .style = UI::StyleId::AskBox,
                            },
                            [&] {
                                b.element({.flags = UI::Flag::Focusable,
                                           .w = UI::Size::grow(),
                                           .h = UI::Size::fit(),
                                           .childDirection = UI::ChildDir::LeftRight,
                                           .padding = UI::Padding::axis(5, 0),
                                           .style = UI::StyleId::AskTextInput,
                                           .onClick = [&] {
                                               Input::endTextInput();
                                               Input::startTextInput();
                                           }},
                                          [&] {
                                              b.text({
                                                  .text = Input::inputText + "|",
                                                  .fontId = 1,
                                                  .scale = 0.75,
                                                  .color = {0, 0, 0, 255},
                                                  .align = UI::AlignMode::Left,
                                              });
                                          });

                                b.element({.flags = UI::Flag::Focusable,
                                           .w = UI::Size::fixed(32),
                                           .h = UI::Size::fixed(32),
                                           .style = UI::StyleId::AskConfirm,
                                           .onClick = [&] {
                                               Input::endTextInput();
                                           }

                                          },
                                          [&] {
                                              b.text({.text = "=", .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});
                                          });
                            });
              });
}

void IngameMenu::build(UI::Builder &b) {
    scale = Render::renderScale;

    const float menuScale = b.scale;
    b.scale = b.ingameScale;

    const float screenW = Render::getWidth();
    const float screenH = Render::getHeight();

    const float screenAspect = screenW / screenH;
    const float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barX = 0.0f, barY = 0.0f;
    if (screenAspect > projectAspect) barX = (screenW - Scratch::projectWidth * scale) / 2.0f;
    else if (screenAspect < projectAspect) barY = (screenH - Scratch::projectHeight * scale) / 2.0f;

    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
              },
              [&] {
                  b.element({
                      .w = UI::Size::grow(),
                      .h = UI::Size::grow(),
                  });

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

                  if (Input::isTextInputActive()) {
                      b.scale = menuScale;
                      b.keyboardNav = true;
                      askMenu(b);
                  } else {
                      b.keyboardNav = false;
                  }
              });
}