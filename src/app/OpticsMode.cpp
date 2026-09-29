#include "app/OpticsMode.hpp"

#include <algorithm>
#include <cmath>

#include "app/Files.hpp"
#include "optics/Material.hpp"
#include "optics/Overlay.hpp"
#include "optics/Presets.hpp"
#include "term/Utf8.hpp"

namespace crt::app {

using optics::Selection;
using term::Key;
using term::KeyEvent;
using term::MouseAction;
using term::MouseButton;
using term::MouseEvent;
namespace theme = ui::theme;

namespace {

constexpr std::size_t kMaxUndo = 100;
constexpr std::uint64_t kMaxRays = 400'000'000;  // the image is fully converged long before this
constexpr float kRotateStep = radians(15.0f);
constexpr float kFineRotateStep = radians(1.0f);
constexpr float kWheelRotateStep = radians(5.0f);

char32_t firstCodePoint(std::string_view utf8) {
    std::size_t pos = 0;
    return utf8.empty() ? U'?' : term::decodeUtf8(utf8, pos);
}

std::string signedFixed(float v, int decimals) { return (v >= 0.0f ? "+" : "") + ui::formatFixed(v, decimals); }

}  // namespace

struct OpticsMode::Screenshot {
    optics::Scene scene;
    optics::LightTracer tracer;
    std::string path;
    Stopwatch clock;
    double duration = 15.0;
    float ev = 0.0f;
    bool fills = true;
};

const std::vector<OpticsMode::Tool>& OpticsMode::tools() {
    static const std::vector<Tool> list = {
        {"box", false, "Box", "□"},       {"circle", false, "Circle", "○"},  {"prism", false, "Prism", "△"},
        {"lens", false, "Lens", "◊"},     {"wall", false, "Wall", "─"},      {"arc", false, "Arc mirror", "("},
        {"point", true, "Point", "●"},    {"spot", true, "Spot", "◈"},       {"laser", true, "Laser", "→"},
        {"beam", true, "Beam", "≡"},
    };
    return list;
}

OpticsMode::OpticsMode(optics::Scene initial, std::string scenePath)
    : scene_(std::move(initial)), scenePath_(std::move(scenePath)) {
    currentMaterial_ = std::max(0, optics::findMaterial("glass"));
    currentColor_ = 0;
    const auto& list = optics::presets();
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].title == scene_.name()) presetIndex_ = static_cast<int>(i);
    }
    updatePreview();
}

OpticsMode::~OpticsMode() = default;

void OpticsMode::invalidate() {
    dirty_ = true;
    exposureValid_ = false;
}

bool OpticsMode::isIdle() const {
    if (screenshot_) return false;
    return paused_ || scene_.lights().empty() || tracer_.raysTraced() >= kMaxRays;
}

// ---------------------------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------------------------

bool OpticsMode::cellToWorld(int column, int row, Vec2& world) const {
    const term::Rect& v = layout_.view;
    if (!v.contains(column, row) || tracer_.width() == 0) return false;
    const float px = static_cast<float>(column - v.x) + 0.5f;
    const float py = style_ == term::ImageStyle::HalfBlock ? static_cast<float>(row - v.y) * 2.0f + 1.0f
                                                           : static_cast<float>(row - v.y) + 0.5f;
    world = tracer_.view().toWorld({px, py});
    return scene_.bounds().inflated(0.5f).contains(world);
}

bool OpticsMode::worldToCell(Vec2 world, int& column, int& row) const {
    const Vec2 p = tracer_.view().toPixel(world);
    const float rowScale = style_ == term::ImageStyle::HalfBlock ? 0.5f : 1.0f;
    column = layout_.view.x + static_cast<int>(std::floor(p.x));
    row = layout_.view.y + static_cast<int>(std::floor(p.y * rowScale));
    return layout_.view.contains(column, row);
}

float OpticsMode::pickTolerance() const {
    const auto& v = tracer_.view();
    const float cellHeightPixels = style_ == term::ImageStyle::HalfBlock ? 2.0f : 1.0f;
    return 1.2f * std::max(1.0f / v.sx, cellHeightPixels / v.sy);
}

float OpticsMode::exposureScale() const { return exposureMultiplier_ * std::exp2(scene_.exposure() + userEv_); }

void OpticsMode::updatePreview() {
    const Tool& t = tools()[static_cast<std::size_t>(tool_)];
    preview_ = t.isLight ? nullptr : optics::makeShape(t.type);
    if (preview_) preview_->setMaterial(currentMaterial_);
}

// ---------------------------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------------------------

void OpticsMode::pushUndo() {
    undo_.push_back(scene_);
    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    redo_.clear();
}

void OpticsMode::undo(AppServices& app) {
    if (undo_.empty()) {
        app.toast("Nothing to undo");
        return;
    }
    redo_.push_back(std::move(scene_));
    scene_ = std::move(undo_.back());
    undo_.pop_back();
    clearSelection();
    markDirty();
    app.toast("Undo");
}

void OpticsMode::redo(AppServices& app) {
    if (redo_.empty()) {
        app.toast("Nothing to redo");
        return;
    }
    undo_.push_back(std::move(scene_));
    scene_ = std::move(redo_.back());
    redo_.pop_back();
    clearSelection();
    markDirty();
    app.toast("Redo");
}

void OpticsMode::clearSelection() {
    selected_ = Selection::none();
    hovered_ = Selection::none();
    drag_ = Drag::None;
}

void OpticsMode::place(Vec2 world, AppServices& app) {
    const Tool& t = tools()[static_cast<std::size_t>(tool_)];
    pushUndo();
    world = scene_.clampToBounds(world);
    if (t.isLight) {
        auto light = optics::makeLight(t.type);
        light->position = world;
        light->color = currentColor_;
        if (std::string_view(t.type) == "laser" && currentColor_ == 0) light->color = optics::findLightColor("red");
        selected_ = scene_.add(std::move(light));
    } else {
        auto shape = optics::makeShape(t.type);
        shape->setPosition(world);
        shape->setMaterial(currentMaterial_);
        selected_ = scene_.add(std::move(shape));
    }
    drag_ = Drag::Aim;
    dragMoved_ = false;
    markDirty();
    app.toast(std::string(t.label) + " placed - drag to aim");
}

void OpticsMode::rotateTarget(float delta) {
    const Selection s = target();
    if (!s) return;
    pushUndo();
    if (auto* shape = scene_.shape(s)) shape->setAngle(shape->angle() + delta);
    if (auto* light = scene_.light(s)) light->angle = wrapAngle(light->angle + delta);
    markDirty();
}

void OpticsMode::scaleTarget(float factor, AppServices& app) {
    const Selection s = target();
    if (!s) {
        app.toast("Select an object to resize it");
        return;
    }
    pushUndo();
    if (auto* shape = scene_.shape(s)) shape->scale(factor);
    if (auto* light = scene_.light(s)) {
        if (light->params().empty()) app.toast("This light has no size");
        light->scale(factor);
    }
    markDirty();
}

void OpticsMode::cycleProperty(int direction, AppServices& app) {
    auto wrap = [direction](int value, std::size_t count) {
        const int n = static_cast<int>(count);
        return ((value + direction) % n + n) % n;
    };
    const Selection s = selected_;
    if (auto* shape = scene_.shape(s)) {
        pushUndo();
        shape->setMaterial(wrap(shape->material(), optics::materialCatalogue().size()));
        currentMaterial_ = shape->material();
        markDirty();
        app.toast("Material: " + optics::material(shape->material()).label);
    } else if (auto* light = scene_.light(s)) {
        pushUndo();
        light->color = wrap(light->color, optics::lightColorCatalogue().size());
        currentColor_ = light->color;
        markDirty();
        app.toast("Light: " + optics::lightColor(light->color).label);
    } else if (tools()[static_cast<std::size_t>(tool_)].isLight) {
        currentColor_ = wrap(currentColor_, optics::lightColorCatalogue().size());
        app.toast("New lights: " + optics::lightColor(currentColor_).label);
    } else {
        currentMaterial_ = wrap(currentMaterial_, optics::materialCatalogue().size());
        app.toast("New shapes: " + optics::material(currentMaterial_).label);
    }
    updatePreview();
}

void OpticsMode::adjustPower(float factor, AppServices& app) {
    auto* light = scene_.light(target());
    if (!light) {
        app.toast("Select a light to change its power");
        return;
    }
    pushUndo();
    light->power = std::clamp(light->power * factor, 0.02f, 50.0f);
    markDirty();
    app.toast("Power " + ui::formatFixed(light->power, 2));
}

void OpticsMode::deleteTarget(AppServices& app) {
    const Selection s = target();
    if (!s) return;
    pushUndo();
    scene_.remove(s);
    clearSelection();
    markDirty();
    app.toast("Deleted");
}

void OpticsMode::duplicateSelection(AppServices& app) {
    if (!selected_) {
        app.toast("Select an object to duplicate it");
        return;
    }
    pushUndo();
    selected_ = scene_.duplicate(selected_, {4.0f, 4.0f});
    markDirty();
    app.toast("Duplicated");
}

void OpticsMode::loadPreset(int index, AppServices& app) {
    const auto& list = optics::presets();
    const int n = static_cast<int>(list.size());
    index = (index % n + n) % n;
    optics::Scene scene;
    if (!optics::makePreset(list[static_cast<std::size_t>(index)].id, scene)) return;
    pushUndo();
    scene_ = std::move(scene);
    presetIndex_ = index;
    clearSelection();
    invalidate();
    app.toast("Demo " + std::to_string(index + 1) + "/" + std::to_string(n) + ": " + scene_.name());
}

void OpticsMode::save(AppServices& app) {
    std::string error;
    if (scene_.saveToFile(scenePath_, error)) app.toast("Saved to " + scenePath_);
    else app.toast("Save failed: " + error);
}

void OpticsMode::load(AppServices& app) {
    optics::Scene loaded;
    std::string error;
    if (!optics::Scene::loadFromFile(scenePath_, loaded, error)) {
        app.toast("Load failed: " + error);
        return;
    }
    pushUndo();
    scene_ = std::move(loaded);
    clearSelection();
    invalidate();
    app.toast("Loaded " + scenePath_);
}

void OpticsMode::startScreenshot(AppServices& app) {
    if (screenshot_) return;
    auto shot = std::make_unique<Screenshot>();
    shot->scene = scene_;
    shot->path = timestampedFileName("optics", ".png");
    shot->ev = scene_.exposure() + userEv_;
    shot->fills = showFills_;
    constexpr int kWidth = 1280;
    const int height = static_cast<int>(std::lround(kWidth * scene_.height() / scene_.width()));
    shot->tracer.configure(kWidth, height, optics::ViewTransform::fit(scene_.bounds(), kWidth, height, 1.0f));
    screenshot_ = std::move(shot);
    app.toast("Rendering " + screenshot_->path + " ...");
}

void OpticsMode::continueScreenshot(AppServices& app, double budgetSeconds) {
    Screenshot& shot = *screenshot_;
    Stopwatch sw;
    while (sw.seconds() < budgetSeconds) shot.tracer.trace(shot.scene, app.pool(), 200'000);
    if (shot.clock.seconds() < shot.duration) return;

    ImageF hdr;
    shot.tracer.resolve(hdr);
    Image8 image = toneMap(hdr, autoExposure(hdr) * std::exp2(shot.ev));
    optics::OverlayOptions options;
    options.fillStrength = shot.fills ? 1.0f : 0.0f;
    optics::drawOverlay(image, shot.scene, shot.tracer.view(), options);
    std::string error;
    if (writePng(shot.path, image, &error)) {
        app.toast("Saved " + shot.path + " (" + ui::formatCount(static_cast<double>(shot.tracer.raysTraced())) + " rays)");
    } else {
        app.toast("Screenshot failed: " + error);
    }
    screenshot_.reset();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

bool OpticsMode::handleEvent(const term::Event& event, AppServices& app) {
    if (const auto* key = std::get_if<KeyEvent>(&event)) return handleKey(*key, app);
    if (const auto* mouse = std::get_if<MouseEvent>(&event)) return handleMouse(*mouse, app);
    return false;
}

bool OpticsMode::handleKey(const KeyEvent& k, AppServices& app) {
    const int toolCount = static_cast<int>(tools().size());
    auto selectTool = [&](int index) {
        tool_ = (index % toolCount + toolCount) % toolCount;
        updatePreview();
        app.toast(std::string("Tool: ") + tools()[static_cast<std::size_t>(tool_)].label);
    };

    switch (k.key) {
        case Key::Escape:
            if (drag_ != Drag::None || selected_) {
                clearSelection();
                return true;
            }
            return false;
        case Key::Up: cycleProperty(-1, app); return true;
        case Key::Down: cycleProperty(+1, app); return true;
        case Key::Left: selectTool(tool_ - 1); return true;
        case Key::Right: selectTool(tool_ + 1); return true;
        case Key::Delete:
        case Key::Backspace: deleteTarget(app); return true;
        case Key::F5: save(app); return true;
        case Key::F9: load(app); return true;
        case Key::Char: break;
        default: return false;
    }

    if (k.mods.ctrl) {
        switch (k.ch) {
            case U'z': undo(app); return true;
            case U'y': redo(app); return true;
            case U's': save(app); return true;
            case U'o': load(app); return true;
            default: return false;
        }
    }

    const char32_t c = k.ch;
    if (c >= U'1' && c <= U'9') {
        selectTool(static_cast<int>(c - U'1'));
        return true;
    }
    switch (c) {
        case U'0': selectTool(9); return true;
        case U'q': rotateTarget(-kRotateStep); return true;
        case U'e': rotateTarget(kRotateStep); return true;
        case U'Q': rotateTarget(-kFineRotateStep); return true;
        case U'E': rotateTarget(kFineRotateStep); return true;
        case U'[': scaleTarget(1.0f / 1.1f, app); return true;
        case U']': scaleTarget(1.1f, app); return true;
        case U'+':
        case U'=': adjustPower(1.25f, app); return true;
        case U'-':
        case U'_': adjustPower(1.0f / 1.25f, app); return true;
        case U',': userEv_ -= 0.5f; app.toast("Exposure " + signedFixed(userEv_, 1) + " EV"); return true;
        case U'.': userEv_ += 0.5f; app.toast("Exposure " + signedFixed(userEv_, 1) + " EV"); return true;
        case U'a':
            autoExposure_ = !autoExposure_;
            app.toast(autoExposure_ ? "Auto exposure on" : "Auto exposure locked");
            return true;
        case U'x': deleteTarget(app); return true;
        case U'd': duplicateSelection(app); return true;
        case U'c':
            pushUndo();
            scene_.clear();
            clearSelection();
            markDirty();
            app.toast("Scene cleared (u to undo)");
            return true;
        case U'n': loadPreset(presetIndex_ + 1, app); return true;
        case U'N': loadPreset(presetIndex_ - 1, app); return true;
        case U' ':
            paused_ = !paused_;
            app.toast(paused_ ? "Paused" : "Resumed");
            return true;
        case U'g':
            showFills_ = !showFills_;
            return true;
        case U'u': undo(app); return true;
        case U'U': redo(app); return true;
        case U'p': startScreenshot(app); return true;
        default: return false;
    }
}

bool OpticsMode::handleMouse(const MouseEvent& m, AppServices& app) {
    Vec2 world;
    mouseInWorld_ = cellToWorld(m.x, m.y, world);
    if (mouseInWorld_) mouseWorld_ = world;

    switch (m.action) {
        case MouseAction::Move: {
            if (drag_ == Drag::None || m.button != MouseButton::Left) {
                drag_ = Drag::None;
                hovered_ = mouseInWorld_ ? scene_.pick(world, pickTolerance()) : Selection::none();
                return true;
            }
            if (!mouseInWorld_) return true;
            if (drag_ == Drag::Move) {
                if (!dragMoved_ && dragSnapshot_) {
                    undo_.push_back(std::move(*dragSnapshot_));
                    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
                    redo_.clear();
                    dragSnapshot_.reset();
                }
                dragMoved_ = true;
                const Vec2 p = scene_.clampToBounds(world + dragOffset_);
                if (auto* shape = scene_.shape(selected_)) shape->setPosition(p);
                if (auto* light = scene_.light(selected_)) light->position = p;
                markDirty();
            } else if (drag_ == Drag::Aim) {
                Vec2 origin;
                if (auto* shape = scene_.shape(selected_)) origin = shape->position();
                if (auto* light = scene_.light(selected_)) origin = light->position;
                const Vec2 d = world - origin;
                if (lengthSquared(d) > square(pickTolerance())) {
                    if (auto* shape = scene_.shape(selected_)) shape->setAngle(angleOf(d));
                    if (auto* light = scene_.light(selected_)) light->angle = angleOf(d);
                    markDirty();
                }
            }
            return true;
        }
        case MouseAction::Press: {
            if (!mouseInWorld_) return false;
            if (m.button == MouseButton::Left) {
                const Selection hit = scene_.pick(world, pickTolerance());
                if (hit) {
                    selected_ = hit;
                    Vec2 origin;
                    if (auto* shape = scene_.shape(hit)) origin = shape->position();
                    if (auto* light = scene_.light(hit)) origin = light->position;
                    dragOffset_ = origin - world;
                    drag_ = Drag::Move;
                    dragMoved_ = false;
                    dragSnapshot_ = std::make_unique<optics::Scene>(scene_);
                } else {
                    place(world, app);
                }
                return true;
            }
            if (m.button == MouseButton::Right) {
                const Selection hit = scene_.pick(world, pickTolerance());
                if (hit) {
                    pushUndo();
                    scene_.remove(hit);
                    clearSelection();
                    markDirty();
                }
                return true;
            }
            return false;
        }
        case MouseAction::Release:
            drag_ = Drag::None;
            dragSnapshot_.reset();
            return true;
        case MouseAction::WheelUp:
        case MouseAction::WheelDown: {
            const float step = m.mods.shift ? kFineRotateStep : kWheelRotateStep;
            rotateTarget(m.action == MouseAction::WheelUp ? -step : step);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------

void OpticsMode::update(const Layout& layout, AppServices& app, double budgetSeconds) {
    layout_ = layout;
    if (style_ != app.imageStyle()) {
        style_ = app.imageStyle();
        dirty_ = true;
    }
    const int w = layout.view.width;
    const int h = term::imageRowsFor(style_, layout.view.height);
    if (w <= 0 || h <= 0) return;
    tracer_.configure(w, h, optics::ViewTransform::fit(scene_.bounds(), w, h, term::pixelAspectFor(style_)));
    if (dirty_) {
        tracer_.reset();
        dirty_ = false;
    }

    if (screenshot_) {
        continueScreenshot(app, budgetSeconds);
        return;
    }
    if (isIdle()) return;

    Stopwatch total;
    std::size_t traced = 0;
    do {
        const double remaining = std::max(0.002, budgetSeconds - total.seconds());
        const auto rays = static_cast<std::size_t>(std::clamp(remaining / secondsPerRay_, 2000.0, 4'000'000.0));
        Stopwatch batch;
        tracer_.trace(scene_, app.pool(), rays);
        const double perRay = batch.seconds() / static_cast<double>(rays);
        secondsPerRay_ = 0.5 * secondsPerRay_ + 0.5 * std::max(perRay, 1e-9);
        traced += rays;
    } while (total.seconds() < budgetSeconds * 0.8);
    raysPerSecond_.add(static_cast<double>(traced) / std::max(total.seconds(), 1e-6));
}

void OpticsMode::draw(term::Canvas& canvas, const Layout& layout, AppServices& /*app*/) {
    layout_ = layout;
    if (tracer_.width() > 0) {
        tracer_.resolve(hdr_);
        if (autoExposure_ && tracer_.raysTraced() > 0 && !scene_.lights().empty()) {
            const float target = autoExposure(hdr_);
            exposureMultiplier_ = exposureValid_ ? std::exp(lerp(std::log(exposureMultiplier_), std::log(target), 0.25f)) : target;
            exposureValid_ = true;
        }
        image_ = toneMap(hdr_, exposureScale());

        optics::OverlayOptions options;
        options.selected = selected_;
        options.hovered = hovered_;
        options.drawLights = false;
        options.fillStrength = showFills_ ? 1.0f : 0.0f;
        options.background = theme::kBackground;
        const bool showPreview = preview_ && mouseInWorld_ && !hovered_ && drag_ == Drag::None;
        if (showPreview) {
            preview_->setPosition(mouseWorld_);
            options.preview = preview_.get();
        }
        optics::drawOverlay(image_, scene_, tracer_.view(), options);
        canvas.blitImage(image_, layout.view.x, layout.view.y, style_);
        drawLights(canvas);
    }
    if (layout.panel.width > 0) drawPanel(canvas, layout.panel);
}

void OpticsMode::drawLights(term::Canvas& canvas) const {
    for (std::size_t i = 0; i < scene_.lights().size(); ++i) {
        const optics::Light& light = *scene_.lights()[i];
        int cx, cy;
        if (!worldToCell(light.position, cx, cy)) continue;
        term::Cell& cell = canvas.at(cx, cy);
        const bool selected = selected_ == Selection::light(i);
        const bool hovered = hovered_ == Selection::light(i);
        cell.bg = mix(cell.fg, cell.bg, 0.5f);
        cell.ch = firstCodePoint(light.glyph());
        cell.fg = optics::lightColor(light.color).spectrum.swatch();
        cell.bold = true;
        if (selected) {
            cell.bg = optics::kSelectionColor;
            cell.fg = {20, 20, 20};
        } else if (hovered) {
            cell.bg = theme::kHighlight;
        }
    }
    // Ghost of the light about to be placed.
    const Tool& t = tools()[static_cast<std::size_t>(tool_)];
    int cx, cy;
    if (t.isLight && mouseInWorld_ && !hovered_ && drag_ == Drag::None && worldToCell(mouseWorld_, cx, cy)) {
        term::Cell& cell = canvas.at(cx, cy);
        cell.ch = firstCodePoint(t.icon);
        cell.fg = mix(optics::lightColor(currentColor_).spectrum.swatch(), cell.bg, 0.4f);
    }
}

void OpticsMode::drawPanel(term::Canvas& canvas, const term::Rect& area) const {
    canvas.fill(area, theme::kPanel);
    ui::PanelWriter panel(canvas, area);

    panel.heading(scene_.name().empty() ? "Scene" : scene_.name());
    panel.heading("Tools");
    const auto& list = tools();
    const int half = static_cast<int>(list.size() + 1) / 2;
    for (int row = 0; row < half && !panel.full(); ++row) {
        const int y = area.y + (area.height - panel.remaining());
        for (int col = 0; col < 2; ++col) {
            const int index = row + col * half;
            if (index >= static_cast<int>(list.size())) continue;
            const Tool& t = list[static_cast<std::size_t>(index)];
            const bool active = index == tool_;
            const int x = area.x + 1 + col * (area.width / 2);
            const std::string key = std::to_string((index + 1) % 10);
            canvas.text(x, y, active ? "▶" : " ", theme::kAccent);
            canvas.text(x + 1, y, key, theme::kDim);
            canvas.text(x + 3, y, t.icon, active ? theme::kAccent : theme::kTitle);
            canvas.text(x + 5, y, t.label, active ? theme::kAccent : theme::kText, area.width / 2 - 6, active);
        }
        panel.skip();
    }

    // Palette: materials for shape tools / selected shapes, light colours otherwise.
    const bool lightPalette = scene_.light(selected_) != nullptr ||
                              (!scene_.shape(selected_) && list[static_cast<std::size_t>(tool_)].isLight);
    int current = lightPalette ? currentColor_ : currentMaterial_;
    if (const auto* shape = scene_.shape(selected_)) current = shape->material();
    if (const auto* light = scene_.light(selected_)) current = light->color;
    const int count = static_cast<int>(lightPalette ? optics::lightColorCatalogue().size() : optics::materialCatalogue().size());
    panel.heading(lightPalette ? "Light colour  ↑↓" : "Material  ↑↓");
    const int reserved = 9;  // selection + stats below
    const int visible = std::clamp(panel.remaining() - reserved, 3, count);
    const int first = std::clamp(current - visible / 2, 0, count - visible);
    for (int i = first; i < first + visible; ++i) {
        if (lightPalette) {
            const auto& c = optics::lightColor(i);
            panel.item(c.label, i == current, c.spectrum.swatch(), true);
        } else {
            const auto& m = optics::material(i);
            panel.item(m.label, i == current, m.tint, true);
        }
    }

    panel.heading("Selection");
    if (const auto* shape = scene_.shape(selected_)) {
        panel.text(std::string(shape->displayName()) + " · " + optics::material(shape->material()).label, theme::kAccent);
        panel.keyValue("pos", ui::formatFixed(shape->position().x, 1) + ", " + ui::formatFixed(shape->position().y, 1) +
                                  "  rot " + ui::formatFixed(degrees(shape->angle()), 0) + "°");
        std::string dims;
        for (const auto& p : shape->params()) dims += p.name + " " + ui::formatFixed(p.value, 1) + "  ";
        panel.keyValue("size", dims);
    } else if (const auto* light = scene_.light(selected_)) {
        panel.text(std::string(light->displayName()) + " · " + optics::lightColor(light->color).label, theme::kAccent);
        panel.keyValue("pos", ui::formatFixed(light->position.x, 1) + ", " + ui::formatFixed(light->position.y, 1) +
                                  "  dir " + ui::formatFixed(degrees(light->angle), 0) + "°");
        std::string extra = "power " + ui::formatFixed(light->power, 2);
        for (const auto& p : light->params()) extra += "  " + p.name + " " + ui::formatFixed(p.value, 1);
        panel.keyValue("", extra);
    } else {
        panel.text("click an object to select it", theme::kDim);
        panel.text("click empty space to place", theme::kDim);
    }

    panel.heading("Render");
    panel.keyValue("rays", ui::formatCount(static_cast<double>(tracer_.raysTraced())) + "  (" +
                               ui::formatCount(raysPerSecond_.value()) + "/s)");
    panel.keyValue("exposure", signedFixed(scene_.exposure() + userEv_, 1) + " EV" + (autoExposure_ ? " auto" : " locked"));
    panel.keyValue("objects", std::to_string(scene_.shapes().size()) + " shapes, " + std::to_string(scene_.lights().size()) +
                                  " lights");
}

std::string OpticsMode::statusLeft() const {
    if (screenshot_) {
        const double progress = std::min(1.0, screenshot_->clock.seconds() / screenshot_->duration);
        return "Rendering " + screenshot_->path + "  " + ui::formatFixed(progress * 100.0, 0) + "%";
    }
    return std::string("2D OPTICS │ ") + tools()[static_cast<std::size_t>(tool_)].label +
           " │ F1 help · Tab 3D · n demos · p screenshot";
}

std::string OpticsMode::statusRight() const {
    std::string s = paused_ ? "paused · " : "";
    s += ui::formatCount(static_cast<double>(tracer_.raysTraced())) + " rays";
    if (!paused_ && !isIdle()) s += " · " + ui::formatCount(raysPerSecond_.value()) + "/s";
    return s;
}

std::vector<ui::HelpSection> OpticsMode::help() const {
    return {
        {"Mouse",
         {{"Left click", "place the current tool / select an object"},
          {"Left drag", "move the object (right after placing: aim it)"},
          {"Right click", "delete the object under the cursor"},
          {"Wheel", "rotate the hovered / selected object (Shift: fine)"}}},
        {"Editing",
         {{"1-9, 0  ← →", "choose tool: box, circle, prism, lens, wall, arc, lights"},
          {"↑ ↓", "material / light colour (selection or next object)"},
          {"q e  /  Q E", "rotate by 15° / 1°"},
          {"[ ]", "resize (shapes, spot cone, beam width)"},
          {"+ -", "light power"},
          {"d  x/Del", "duplicate / delete"},
          {"u U  Ctrl+Z Ctrl+Y", "undo / redo"},
          {"c", "clear the scene"}}},
        {"View & files",
         {{", .", "exposure -/+ 0.5 EV      a  toggle auto exposure"},
          {"g  Space  v", "object fills / pause / half-block <-> ASCII"},
          {"n N", "next / previous demo scene"},
          {"Ctrl+S Ctrl+O", "save / load " + scenePath_ + " (also F5 / F9)"},
          {"p", "render a 1280px PNG screenshot (15 s)"},
          {"Tab  Esc Esc", "switch to 3D  /  quit"}}},
    };
}

}  // namespace crt::app
