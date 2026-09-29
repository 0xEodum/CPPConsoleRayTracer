#include "app/App.hpp"

#include <algorithm>

#include "app/OpticsMode.hpp"
#include "app/PathTraceMode.hpp"

namespace crt::app {

using term::Key;
using term::KeyEvent;
namespace theme = ui::theme;

namespace {

constexpr int kMinColumns = 40;
constexpr int kMinRows = 12;
constexpr double kInteractiveWindow = 0.6;  // seconds after the last input that count as "interacting"
constexpr double kToastSeconds = 2.5;

}  // namespace

App::App(AppOptions options)
    : pool_(options.threads), presenter_(options.colorMode), style_(options.imageStyle) {
    modes_.push_back(std::make_unique<OpticsMode>(std::move(options.opticsScene), options.opticsScenePath));
    modes_.push_back(std::make_unique<PathTraceMode>(options.preset3d));
    active_ = options.start3d ? 1 : 0;
}

App::~App() = default;

void App::toast(std::string message) {
    toast_ = std::move(message);
    toastUntil_ = time() + kToastSeconds;
}

Layout App::computeLayout() const {
    Layout layout;
    const int columns = canvas_.width();
    const int rows = canvas_.height();
    const int panelWidth = columns >= 110 ? 36 : (columns >= 84 ? 30 : 0);
    layout.statusRow = rows - 1;
    layout.view = {0, 0, columns - panelWidth, rows - 1};
    layout.panel = {columns - panelWidth, 0, panelWidth, rows - 1};
    return layout;
}

int App::run() {
    terminal_ = term::Terminal::create();
    const term::TerminalSize initial = terminal_->size();
    canvas_.resize(initial.columns, initial.rows);
    toast("Welcome! F1 or h shows all controls");

    std::vector<term::Event> events;
    double lastInput = -100.0;
    double lastPresent = -100.0;
    while (!quit_) {
        const bool busy = !mode().isIdle() || time() < toastUntil_;
        events.clear();
        terminal_->poll(events, busy ? 0 : 50);
        for (const auto& e : events) handleEvent(e);
        if (quit_) break;
        if (!events.empty()) lastInput = time();

        const term::TerminalSize size = terminal_->size();
        if (size.columns != canvas_.width() || size.rows != canvas_.height()) {
            canvas_.resize(size.columns, size.rows);
            presenter_.invalidate();
        }
        const bool tooSmall = canvas_.width() < kMinColumns || canvas_.height() < kMinRows;
        const Layout layout = computeLayout();
        const bool interactive = time() - lastInput < kInteractiveWindow;

        if (!tooSmall) mode().update(layout, *this, interactive ? 0.022 : 0.06);

        // Present at up to 30 fps while interacting; otherwise ~8 fps is plenty for a converging image.
        const double interval = interactive ? 1.0 / 30.0 : 1.0 / 8.0;
        if (!events.empty() || time() - lastPresent >= interval) {
            presenter_.setTolerance(interactive || mode().isIdle() ? 0 : 3);
            if (tooSmall) {
                canvas_.clear(theme::kBackground);
                ui::drawMessageBox(canvas_, "Console Ray Tracer",
                                   {"Terminal too small: " + std::to_string(canvas_.width()) + "x" +
                                        std::to_string(canvas_.height()),
                                    "Please enlarge it to at least " + std::to_string(kMinColumns) + "x" +
                                        std::to_string(kMinRows) + ".",
                                    "Ctrl+C quits."});
                terminal_->write(presenter_.render(canvas_));
            } else {
                drawFrame(layout);
            }
            lastPresent = time();
        }
    }
    terminal_.reset();  // restores the terminal before anything else is printed
    return 0;
}

void App::drawFrame(const Layout& layout) {
    canvas_.clear(theme::kBackground);
    mode().draw(canvas_, layout, *this);
    ui::drawStatusBar(canvas_, layout.statusRow, mode().statusLeft(), mode().statusRight());
    if (time() < toastUntil_) ui::drawToast(canvas_, layout.statusRow - 1, toast_);
    if (showHelp_) {
        auto sections = mode().help();
        sections.push_back({"Application",
                            {{"Tab", "switch between 2D optics and 3D path tracing"},
                             {"v", "toggle half-block / ASCII-art image style"},
                             {"F1  h  ?", "toggle this help"},
                             {"Esc Esc  Ctrl+C  F10", "quit"}}});
        ui::drawHelpOverlay(canvas_, std::string(mode().name()) + " - controls", sections);
    }
    terminal_->write(presenter_.render(canvas_));
}

bool App::handleGlobalKey(const KeyEvent& k) {
    if (k.isCtrl(U'c') || k.key == Key::F10) {
        quit_ = true;
        return true;
    }
    if (showHelp_) {  // any key closes the help overlay
        showHelp_ = false;
        return true;
    }
    if (k.key == Key::F1 || k.is(U'h') || k.is(U'?')) {
        showHelp_ = true;
        return true;
    }
    if (k.key == Key::Tab) {
        active_ = (active_ + 1) % modes_.size();
        toast(std::string("Mode: ") + std::string(mode().name()));
        return true;
    }
    if (k.is(U'v')) {
        style_ = style_ == term::ImageStyle::HalfBlock ? term::ImageStyle::Ascii : term::ImageStyle::HalfBlock;
        for (auto& m : modes_) m->invalidate();
        toast(style_ == term::ImageStyle::Ascii ? "ASCII-art rendering" : "Half-block rendering");
        return true;
    }
    if (k.isCtrl(U'l')) {
        presenter_.invalidate();
        return true;
    }
    return false;
}

void App::handleEvent(const term::Event& event) {
    if (std::holds_alternative<term::ResizeEvent>(event)) {
        presenter_.invalidate();
        return;
    }
    if (const auto* key = std::get_if<KeyEvent>(&event)) {
        if (handleGlobalKey(*key)) return;
        if (mode().handleEvent(event, *this)) return;
        if (key->key == Key::Escape) {
            if (time() - escapeArmedAt_ < 2.0) {
                quit_ = true;
            } else {
                escapeArmedAt_ = time();
                toast("Press Esc again to quit");
            }
        }
        return;
    }
    if (const auto* mouse = std::get_if<term::MouseEvent>(&event)) {
        if (showHelp_) {
            if (mouse->action == term::MouseAction::Press) showHelp_ = false;
            return;
        }
        mode().handleEvent(event, *this);
    }
}

}  // namespace crt::app
