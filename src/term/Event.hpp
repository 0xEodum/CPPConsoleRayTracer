#pragma once

#include <variant>

namespace crt::term {

enum class Key {
    Char,  ///< printable character (see KeyEvent::ch), also Ctrl+letter combinations
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Up,
    Down,
    Left,
    Right,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
};

struct Modifiers {
    bool shift = false;
    bool alt = false;
    bool ctrl = false;
};

struct KeyEvent {
    Key key = Key::Char;
    char32_t ch = 0;  ///< for Key::Char: the character (lower-case letter when combined with Ctrl)
    Modifiers mods;

    bool is(char32_t c) const { return key == Key::Char && ch == c && !mods.ctrl && !mods.alt; }
    bool isCtrl(char32_t c) const { return key == Key::Char && ch == c && mods.ctrl; }
};

enum class MouseButton { None, Left, Middle, Right };
enum class MouseAction { Press, Release, Move, WheelUp, WheelDown };

struct MouseEvent {
    MouseAction action = MouseAction::Move;
    MouseButton button = MouseButton::None;  ///< for Move: the button held while dragging, if any
    int x = 0;                               ///< zero-based terminal column
    int y = 0;                               ///< zero-based terminal row
    Modifiers mods;
};

struct ResizeEvent {
    int columns = 0;
    int rows = 0;
};

using Event = std::variant<KeyEvent, MouseEvent, ResizeEvent>;

}  // namespace crt::term
