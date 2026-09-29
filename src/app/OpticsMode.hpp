#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/Mode.hpp"
#include "core/Timer.hpp"
#include "optics/LightTracer.hpp"
#include "optics/Scene.hpp"

namespace crt::app {

/// Interactive 2D optics sandbox: place and edit shapes and lights, watch light propagate.
class OpticsMode final : public Mode {
public:
    /// `scenePath` is used by save/load; `initial` is the scene shown at start.
    OpticsMode(optics::Scene initial, std::string scenePath);
    ~OpticsMode() override;

    std::string_view name() const override { return "2D Optics"; }
    bool handleEvent(const term::Event& event, AppServices& app) override;
    void update(const Layout& layout, AppServices& app, double budgetSeconds) override;
    void draw(term::Canvas& canvas, const Layout& layout, AppServices& app) override;
    std::string statusLeft() const override;
    std::string statusRight() const override;
    std::vector<ui::HelpSection> help() const override;
    bool isIdle() const override;
    void invalidate() override;

private:
    struct Tool {
        const char* type;
        bool isLight;
        const char* label;
        const char* icon;
    };
    static const std::vector<Tool>& tools();

    enum class Drag { None, Move, Aim };

    bool handleKey(const term::KeyEvent& key, AppServices& app);
    bool handleMouse(const term::MouseEvent& mouse, AppServices& app);

    // Editing (every mutation goes through pushUndo() first, then markDirty()).
    void pushUndo();
    void undo(AppServices& app);
    void redo(AppServices& app);
    void markDirty() { dirty_ = true; }
    void clearSelection();
    optics::Selection target() const { return selected_ ? selected_ : hovered_; }
    void place(Vec2 world, AppServices& app);
    void rotateTarget(float radians);
    void scaleTarget(float factor, AppServices& app);
    void cycleProperty(int direction, AppServices& app);
    void adjustPower(float factor, AppServices& app);
    void deleteTarget(AppServices& app);
    void duplicateSelection(AppServices& app);
    void loadPreset(int index, AppServices& app);
    void save(AppServices& app);
    void load(AppServices& app);
    void startScreenshot(AppServices& app);
    void continueScreenshot(AppServices& app, double budgetSeconds);

    bool cellToWorld(int column, int row, Vec2& world) const;
    bool worldToCell(Vec2 world, int& column, int& row) const;
    float pickTolerance() const;
    float exposureScale() const;
    void updatePreview();

    void drawLights(term::Canvas& canvas) const;
    void drawPanel(term::Canvas& canvas, const term::Rect& area) const;

    optics::Scene scene_;
    std::string scenePath_;
    std::vector<optics::Scene> undo_;
    std::vector<optics::Scene> redo_;
    int presetIndex_ = -1;

    int tool_ = 0;
    int currentMaterial_ = 0;
    int currentColor_ = 0;
    optics::Selection selected_;
    optics::Selection hovered_;
    Drag drag_ = Drag::None;
    Vec2 dragOffset_;
    bool dragMoved_ = false;
    std::unique_ptr<optics::Scene> dragSnapshot_;
    Vec2 mouseWorld_;
    bool mouseInWorld_ = false;
    std::unique_ptr<optics::Shape> preview_;

    optics::LightTracer tracer_;
    ImageF hdr_;
    Image8 image_;
    Layout layout_;
    term::ImageStyle style_ = term::ImageStyle::HalfBlock;
    bool dirty_ = true;
    bool paused_ = false;
    bool showFills_ = true;
    bool autoExposure_ = true;
    bool exposureValid_ = false;
    float exposureMultiplier_ = 1.0f;
    float userEv_ = 0.0f;
    double secondsPerRay_ = 2e-6;
    RunningAverage raysPerSecond_{0.2};

    struct Screenshot;
    std::unique_ptr<Screenshot> screenshot_;
};

}  // namespace crt::app
