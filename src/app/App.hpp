#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/Mode.hpp"
#include "core/ThreadPool.hpp"
#include "core/Timer.hpp"
#include "optics/Scene.hpp"
#include "term/Presenter.hpp"
#include "term/Terminal.hpp"

namespace crt::app {

struct AppOptions {
    bool start3d = false;
    optics::Scene opticsScene;
    std::string opticsScenePath = "optics.scene";
    std::string preset3d = "showcase";
    term::ColorMode colorMode = term::ColorMode::TrueColor;
    term::ImageStyle imageStyle = term::ImageStyle::HalfBlock;
    std::size_t threads = 0;  ///< 0 = all hardware threads
};

/// The interactive shell: owns the terminal, the frame loop and the modes, and routes input.
class App final : public AppServices {
public:
    explicit App(AppOptions options);
    ~App() override;

    /// Runs until the user quits. Returns the process exit code.
    int run();

    // AppServices
    ThreadPool& pool() override { return pool_; }
    term::ImageStyle imageStyle() const override { return style_; }
    void toast(std::string message) override;
    double time() const override { return clock_.seconds(); }

private:
    Mode& mode() { return *modes_[active_]; }
    void handleEvent(const term::Event& event);
    bool handleGlobalKey(const term::KeyEvent& key);
    Layout computeLayout() const;
    void drawFrame(const Layout& layout);

    ThreadPool pool_;
    std::unique_ptr<term::Terminal> terminal_;
    term::Presenter presenter_;
    term::Canvas canvas_;
    std::vector<std::unique_ptr<Mode>> modes_;
    std::size_t active_ = 0;
    term::ImageStyle style_;
    Stopwatch clock_;

    bool quit_ = false;
    bool showHelp_ = false;
    double escapeArmedAt_ = -100.0;
    std::string toast_;
    double toastUntil_ = 0.0;
};

}  // namespace crt::app
