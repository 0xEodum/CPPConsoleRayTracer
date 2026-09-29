#include "app/PathTraceMode.hpp"

#include <algorithm>
#include <cmath>

#include "app/Files.hpp"
#include "rt/Presets.hpp"

namespace crt::app {

using term::Key;
using term::KeyEvent;
using term::MouseAction;
using term::MouseButton;
using term::MouseEvent;
namespace theme = ui::theme;

namespace {

constexpr float kLookStep = radians(3.0f);
constexpr float kMaxPitch = radians(89.0f);
constexpr int kBounceOptions[] = {2, 4, 8, 16};

std::string signedFixed(float v, int decimals) { return (v >= 0.0f ? "+" : "") + ui::formatFixed(v, decimals); }

}  // namespace

struct PathTraceMode::Screenshot {
    rt::PathTracer tracer;
    rt::Camera camera;
    std::string path;
    Stopwatch clock;
    double maxSeconds = 25.0;
    int targetSamples = 256;
};

PathTraceMode::PathTraceMode(std::string presetId) {
    const auto& list = rt::presets();
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].id == presetId) presetIndex_ = static_cast<int>(i);
    }
    rt::makePreset(list[static_cast<std::size_t>(presetIndex_)].id, scene_);
    camera_ = scene_.camera;
}

PathTraceMode::~PathTraceMode() = default;

bool PathTraceMode::isIdle() const { return !screenshot_ && tracer_.samples() >= kMaxSamples; }

void PathTraceMode::loadPreset(int index, AppServices& app) {
    const auto& list = rt::presets();
    const int n = static_cast<int>(list.size());
    presetIndex_ = (index % n + n) % n;
    rt::makePreset(list[static_cast<std::size_t>(presetIndex_)].id, scene_);
    camera_ = scene_.camera;
    userEv_ = 0.0f;
    tracer_.reset();
    app.toast("Scene " + std::to_string(presetIndex_ + 1) + "/" + std::to_string(n) + ": " + scene_.name);
}

void PathTraceMode::focusAt(int column, int row, AppServices& app) {
    const term::Rect& v = layout_.view;
    if (!v.contains(column, row) || tracer_.width() == 0) return;
    const float px = static_cast<float>(column - v.x) + 0.5f;
    const float py = style_ == term::ImageStyle::HalfBlock ? static_cast<float>(row - v.y) * 2.0f + 1.0f
                                                           : static_cast<float>(row - v.y) + 0.5f;
    rt::Camera pinhole = camera_;
    pinhole.aperture = 0.0f;
    Pcg32 rng;
    const rt::Ray ray = pinhole.generateRay(px, py, tracer_.width(), tracer_.height(), tracer_.pixelAspect(), rng);
    rt::Hit hit;
    if (!scene_.intersect(ray, kInfinity, hit)) {
        app.toast("Nothing to focus on there");
        return;
    }
    camera_.focusDistance = hit.t * dot(ray.direction, camera_.forward());
    if (camera_.aperture <= 0.0f) camera_.aperture = 0.08f;
    app.toast("Focus at " + ui::formatFixed(camera_.focusDistance, 2) + " (aperture " + ui::formatFixed(camera_.aperture, 2) +
              ", z/x to change)");
}

void PathTraceMode::startScreenshot(AppServices& app) {
    if (screenshot_) return;
    auto shot = std::make_unique<Screenshot>();
    shot->camera = camera_;
    shot->path = timestampedFileName("render", ".png");
    shot->tracer.settings() = tracer_.settings();
    shot->tracer.configure(1280, 720, 1.0f);
    screenshot_ = std::move(shot);
    app.toast("Rendering " + screenshot_->path + " ...");
}

void PathTraceMode::continueScreenshot(AppServices& app, double budgetSeconds) {
    Screenshot& shot = *screenshot_;
    Stopwatch sw;
    while (sw.seconds() < budgetSeconds && shot.tracer.samples() < shot.targetSamples) {
        shot.tracer.renderPass(scene_, shot.camera, app.pool(), 1);
    }
    if (shot.tracer.samples() < shot.targetSamples && shot.clock.seconds() < shot.maxSeconds) return;

    ImageF hdr;
    shot.tracer.resolve(hdr);
    std::string error;
    if (writePng(shot.path, toneMap(hdr, std::exp2(scene_.exposure + userEv_)), &error)) {
        app.toast("Saved " + shot.path + " (" + std::to_string(shot.tracer.samples()) + " spp)");
    } else {
        app.toast("Screenshot failed: " + error);
    }
    screenshot_.reset();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

bool PathTraceMode::handleEvent(const term::Event& event, AppServices& app) {
    if (const auto* key = std::get_if<KeyEvent>(&event)) return handleKey(*key, app);
    if (const auto* mouse = std::get_if<MouseEvent>(&event)) return handleMouse(*mouse, app);
    return false;
}

bool PathTraceMode::handleKey(const KeyEvent& k, AppServices& app) {
    auto look = [this](float dyaw, float dpitch) {
        camera_.yaw = wrapAngle(camera_.yaw + dyaw);
        camera_.pitch = std::clamp(camera_.pitch + dpitch, -kMaxPitch, kMaxPitch);
    };
    switch (k.key) {
        case Key::Left: look(kLookStep, 0.0f); return true;
        case Key::Right: look(-kLookStep, 0.0f); return true;
        case Key::Up: look(0.0f, kLookStep); return true;
        case Key::Down: look(0.0f, -kLookStep); return true;
        case Key::PageUp: camera_.move({0.0f, moveSpeed_, 0.0f}); return true;
        case Key::PageDown: camera_.move({0.0f, -moveSpeed_, 0.0f}); return true;
        case Key::Char: break;
        default: return false;
    }
    if (k.mods.ctrl || k.mods.alt) return false;

    const char32_t lower = k.ch >= U'A' && k.ch <= U'Z' ? k.ch - U'A' + U'a' : k.ch;
    const float step = moveSpeed_ * (k.ch >= U'A' && k.ch <= U'Z' ? 4.0f : 1.0f);
    switch (lower) {
        case U'w': camera_.move({0.0f, 0.0f, step}); return true;
        case U's': camera_.move({0.0f, 0.0f, -step}); return true;
        case U'a': camera_.move({-step, 0.0f, 0.0f}); return true;
        case U'd': camera_.move({step, 0.0f, 0.0f}); return true;
        case U'r': camera_.move({0.0f, step, 0.0f}); return true;
        case U'f': camera_.move({0.0f, -step, 0.0f}); return true;
        default: break;
    }
    switch (k.ch) {
        case U'[': camera_.verticalFov = std::max(10.0f, camera_.verticalFov - 5.0f); return true;
        case U']': camera_.verticalFov = std::min(110.0f, camera_.verticalFov + 5.0f); return true;
        case U'z':
            camera_.aperture = camera_.aperture < 0.012f ? 0.0f : camera_.aperture / 1.6f;
            app.toast(camera_.aperture > 0.0f ? "Aperture " + ui::formatFixed(camera_.aperture, 3) : "Depth of field off");
            return true;
        case U'x':
            camera_.aperture = camera_.aperture <= 0.0f ? 0.02f : std::min(1.0f, camera_.aperture * 1.6f);
            app.toast("Aperture " + ui::formatFixed(camera_.aperture, 3));
            return true;
        case U't': focusAt(layout_.view.x + layout_.view.width / 2, layout_.view.y + layout_.view.height / 2, app); return true;
        case U',': userEv_ -= 0.5f; app.toast("Exposure " + signedFixed(userEv_, 1) + " EV"); return true;
        case U'.': userEv_ += 0.5f; app.toast("Exposure " + signedFixed(userEv_, 1) + " EV"); return true;
        case U'+':
        case U'=': moveSpeed_ = std::min(4.0f, moveSpeed_ * 1.5f); app.toast("Speed " + ui::formatFixed(moveSpeed_, 2)); return true;
        case U'-':
        case U'_': moveSpeed_ = std::max(0.02f, moveSpeed_ / 1.5f); app.toast("Speed " + ui::formatFixed(moveSpeed_, 2)); return true;
        case U'b': {
            int& depth = tracer_.settings().maxDepth;
            const auto* it = std::find(std::begin(kBounceOptions), std::end(kBounceOptions), depth);
            depth = (it == std::end(kBounceOptions) || it + 1 == std::end(kBounceOptions)) ? kBounceOptions[0] : *(it + 1);
            tracer_.reset();
            app.toast("Max bounces: " + std::to_string(depth));
            return true;
        }
        case U'c': camera_ = scene_.camera; app.toast("Camera reset"); return true;
        case U'n': loadPreset(presetIndex_ + 1, app); return true;
        case U'N': loadPreset(presetIndex_ - 1, app); return true;
        case U'p': startScreenshot(app); return true;
        default: return false;
    }
}

bool PathTraceMode::handleMouse(const MouseEvent& m, AppServices& app) {
    switch (m.action) {
        case MouseAction::Press:
            if (m.button == MouseButton::Left && layout_.view.contains(m.x, m.y)) {
                dragging_ = true;
                dragMoved_ = false;
                dragX_ = m.x;
                dragY_ = m.y;
                return true;
            }
            return false;
        case MouseAction::Move:
            if (!dragging_ || m.button != MouseButton::Left) {
                dragging_ = false;
                return false;
            }
            if (m.x != dragX_ || m.y != dragY_) {
                camera_.yaw = wrapAngle(camera_.yaw - radians(2.0f) * static_cast<float>(m.x - dragX_));
                camera_.pitch = std::clamp(camera_.pitch - radians(4.0f) * static_cast<float>(m.y - dragY_), -kMaxPitch, kMaxPitch);
                dragX_ = m.x;
                dragY_ = m.y;
                dragMoved_ = true;
            }
            return true;
        case MouseAction::Release:
            if (dragging_ && !dragMoved_) focusAt(m.x, m.y, app);
            dragging_ = false;
            return true;
        case MouseAction::WheelUp: camera_.move({0.0f, 0.0f, moveSpeed_}); return true;
        case MouseAction::WheelDown: camera_.move({0.0f, 0.0f, -moveSpeed_}); return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------

void PathTraceMode::update(const Layout& layout, AppServices& app, double budgetSeconds) {
    layout_ = layout;
    style_ = app.imageStyle();
    const int w = layout.view.width;
    const int h = term::imageRowsFor(style_, layout.view.height);
    if (w <= 0 || h <= 0) return;
    tracer_.configure(w, h, term::pixelAspectFor(style_));
    if (camera_ != renderedCamera_) {
        tracer_.reset();
        renderedCamera_ = camera_;
    }

    if (screenshot_) {
        continueScreenshot(app, budgetSeconds);
        return;
    }
    if (tracer_.samples() >= kMaxSamples) return;

    Stopwatch total;
    int done = 0;
    do {
        const double remaining = std::max(0.0, budgetSeconds - total.seconds());
        const int spp = tracer_.samples() == 0 ? 1 : std::clamp(static_cast<int>(remaining / secondsPerSample_), 1, 16);
        Stopwatch pass;
        tracer_.renderPass(scene_, camera_, app.pool(), spp);
        secondsPerSample_ = 0.5 * secondsPerSample_ + 0.5 * std::max(pass.seconds() / spp, 1e-6);
        done += spp;
    } while (total.seconds() < budgetSeconds * 0.8 && tracer_.samples() < kMaxSamples);
    samplesPerSecond_.add(static_cast<double>(done) / std::max(total.seconds(), 1e-6));
}

void PathTraceMode::draw(term::Canvas& canvas, const Layout& layout, AppServices& /*app*/) {
    layout_ = layout;
    if (tracer_.width() > 0) {
        tracer_.resolve(hdr_);
        image_ = toneMap(hdr_, std::exp2(scene_.exposure + userEv_));
        canvas.blitImage(image_, layout.view.x, layout.view.y, style_);
    }
    if (layout.panel.width > 0) drawPanel(canvas, layout.panel);
}

void PathTraceMode::drawPanel(term::Canvas& canvas, const term::Rect& area) const {
    canvas.fill(area, theme::kPanel);
    ui::PanelWriter panel(canvas, area);
    panel.heading(scene_.name);
    panel.wrapped(scene_.description);

    panel.heading("Camera");
    const Vec3& p = camera_.position;
    panel.keyValue("pos", ui::formatFixed(p.x, 2) + ", " + ui::formatFixed(p.y, 2) + ", " + ui::formatFixed(p.z, 2));
    panel.keyValue("look", "yaw " + ui::formatFixed(degrees(camera_.yaw), 0) + "°  pitch " +
                               ui::formatFixed(degrees(camera_.pitch), 0) + "°");
    panel.keyValue("lens", "fov " + ui::formatFixed(camera_.verticalFov, 0) + "°  " +
                               (camera_.aperture > 0.0f ? "f " + ui::formatFixed(camera_.focusDistance, 2) + " ap " +
                                                              ui::formatFixed(camera_.aperture, 2)
                                                        : std::string("pinhole")));

    panel.heading("Render");
    const bool done = tracer_.samples() >= kMaxSamples;
    panel.keyValue("samples", std::to_string(tracer_.samples()) + (done ? " (converged)" : " / px"),
                   done ? theme::kGood : theme::kText);
    panel.keyValue("speed", ui::formatFixed(samplesPerSecond_.value(), 0) + " spp/s  " +
                                ui::formatCount(samplesPerSecond_.value() * tracer_.width() * tracer_.height()) + " paths/s");
    panel.keyValue("bounces", std::to_string(tracer_.settings().maxDepth) + "   exposure " +
                                  signedFixed(scene_.exposure + userEv_, 1) + " EV");
    panel.keyValue("scene", std::to_string(scene_.primitiveCount()) + " primitives, BVH " +
                                std::to_string(scene_.bvh().nodeCount()) + " nodes");

    panel.heading("Controls");
    panel.text("WASD move   R/F up/down", theme::kDim);
    panel.text("arrows / drag  look around", theme::kDim);
    panel.text("click  focus    z/x aperture", theme::kDim);
    panel.text("n  next scene   p  screenshot", theme::kDim);
}

std::string PathTraceMode::statusLeft() const {
    if (screenshot_) {
        return "Rendering " + screenshot_->path + "  " + std::to_string(screenshot_->tracer.samples()) + "/" +
               std::to_string(screenshot_->targetSamples) + " spp";
    }
    return "3D PATH TRACER │ " + scene_.name + " │ F1 help · Tab 2D · WASD/drag · n scenes · p screenshot";
}

std::string PathTraceMode::statusRight() const {
    return std::to_string(tracer_.samples()) + " spp · " + ui::formatFixed(samplesPerSecond_.value(), 0) + " spp/s";
}

std::vector<ui::HelpSection> PathTraceMode::help() const {
    return {
        {"Camera",
         {{"W A S D", "move (Shift: 4x faster)          + -  movement speed"},
          {"R F  PgUp PgDn", "move up / down"},
          {"← → ↑ ↓", "look around (or drag with the left mouse button)"},
          {"Wheel", "move forward / backward"},
          {"[ ]", "field of view"},
          {"c", "reset the camera"}}},
        {"Lens & image",
         {{"Left click", "focus on the clicked object (enables depth of field)"},
          {"t", "focus on the object in the centre"},
          {"z x", "aperture smaller / larger (depth of field)"},
          {", .", "exposure -/+ 0.5 EV"},
          {"b", "max bounces: 2 / 4 / 8 / 16"},
          {"v", "half-block <-> ASCII rendering"}}},
        {"Scenes & files",
         {{"n N", "next / previous scene"},
          {"p", "render a 1280x720 PNG (256 spp)"},
          {"Tab  Esc Esc", "switch to 2D  /  quit"}}},
    };
}

}  // namespace crt::app
