#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/Mode.hpp"
#include "core/Timer.hpp"
#include "rt/PathTracer.hpp"
#include "rt/Scene.hpp"

namespace crt::app {

/// Interactive 3D path tracer with a free-flying camera and progressive refinement.
class PathTraceMode final : public Mode {
public:
    explicit PathTraceMode(std::string presetId = "showcase");
    ~PathTraceMode() override;

    std::string_view name() const override { return "3D Path tracer"; }
    bool handleEvent(const term::Event& event, AppServices& app) override;
    void update(const Layout& layout, AppServices& app, double budgetSeconds) override;
    void draw(term::Canvas& canvas, const Layout& layout, AppServices& app) override;
    std::string statusLeft() const override;
    std::string statusRight() const override;
    std::vector<ui::HelpSection> help() const override;
    bool isIdle() const override;
    void invalidate() override { tracer_.reset(); }

    static constexpr int kMaxSamples = 4096;

private:
    bool handleKey(const term::KeyEvent& key, AppServices& app);
    bool handleMouse(const term::MouseEvent& mouse, AppServices& app);
    void loadPreset(int index, AppServices& app);
    void focusAt(int column, int row, AppServices& app);
    void startScreenshot(AppServices& app);
    void continueScreenshot(AppServices& app, double budgetSeconds);
    void drawPanel(term::Canvas& canvas, const term::Rect& area) const;

    rt::Scene scene_;
    int presetIndex_ = 0;
    rt::Camera camera_;
    rt::Camera renderedCamera_;
    rt::PathTracer tracer_;
    ImageF hdr_;
    Image8 image_;
    Layout layout_;
    term::ImageStyle style_ = term::ImageStyle::HalfBlock;
    float userEv_ = 0.0f;
    float moveSpeed_ = 0.25f;

    bool dragging_ = false;
    bool dragMoved_ = false;
    int dragX_ = 0;
    int dragY_ = 0;

    RunningAverage samplesPerSecond_{0.2};
    double secondsPerSample_ = 0.01;

    struct Screenshot;
    std::unique_ptr<Screenshot> screenshot_;
};

}  // namespace crt::app
