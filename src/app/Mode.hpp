#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/ThreadPool.hpp"
#include "term/Canvas.hpp"
#include "term/Event.hpp"
#include "ui/Widgets.hpp"

namespace crt::app {

/// Services the application shell offers to its modes.
class AppServices {
public:
    virtual ~AppServices() = default;
    virtual ThreadPool& pool() = 0;
    virtual term::ImageStyle imageStyle() const = 0;
    /// Shows a short transient message above the status bar.
    virtual void toast(std::string message) = 0;
    /// Seconds since application start.
    virtual double time() const = 0;
};

/// Screen layout computed by the shell for the current terminal size.
struct Layout {
    term::Rect view;    ///< where the rendered image goes
    term::Rect panel;   ///< side panel (width 0 when the terminal is too narrow)
    int statusRow = 0;  ///< last terminal row
};

/// One interactive screen of the application (State pattern: the shell forwards everything to
/// the active mode). Modes own their scene and renderer and keep running in the background only
/// while active.
class Mode {
public:
    virtual ~Mode() = default;

    virtual std::string_view name() const = 0;

    /// Returns true if the event was consumed.
    virtual bool handleEvent(const term::Event& event, AppServices& app) = 0;

    /// Spends up to `budgetSeconds` of compute on refining the image.
    virtual void update(const Layout& layout, AppServices& app, double budgetSeconds) = 0;

    virtual void draw(term::Canvas& canvas, const Layout& layout, AppServices& app) = 0;

    /// Status line: left (hints) and right (statistics) parts.
    virtual std::string statusLeft() const = 0;
    virtual std::string statusRight() const = 0;

    virtual std::vector<ui::HelpSection> help() const = 0;

    /// True when there is nothing left to compute (the shell can then sleep longer).
    virtual bool isIdle() const = 0;

    /// Called when the image style changes, the mode becomes active, etc.
    virtual void invalidate() = 0;
};

}  // namespace crt::app
