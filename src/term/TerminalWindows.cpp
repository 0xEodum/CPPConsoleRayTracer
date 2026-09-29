#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <stdexcept>
#include <string>

#include "term/Terminal.hpp"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

namespace crt::term {
namespace {

constexpr char kEnterSequence[] = "\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[2J";
constexpr char kLeaveSequence[] = "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";

struct SavedState {
    HANDLE in = INVALID_HANDLE_VALUE;
    HANDLE out = INVALID_HANDLE_VALUE;
    DWORD inMode = 0;
    DWORD outMode = 0;
    UINT inCodePage = 0;
    UINT outCodePage = 0;
};

SavedState gSaved;
std::atomic<bool> gActive{false};

void restoreConsole() {
    if (!gActive.exchange(false)) return;
    DWORD written = 0;
    WriteConsoleA(gSaved.out, kLeaveSequence, static_cast<DWORD>(sizeof(kLeaveSequence) - 1), &written, nullptr);
    SetConsoleMode(gSaved.in, gSaved.inMode);
    SetConsoleMode(gSaved.out, gSaved.outMode);
    SetConsoleCP(gSaved.inCodePage);
    SetConsoleOutputCP(gSaved.outCodePage);
}

BOOL WINAPI onConsoleControl(DWORD type) {
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT || type == CTRL_BREAK_EVENT) {
        restoreConsole();
    }
    return FALSE;  // let the default handler terminate the process
}

Modifiers modifiersFrom(DWORD state) {
    Modifiers m;
    m.shift = (state & SHIFT_PRESSED) != 0;
    m.ctrl = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    m.alt = (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    return m;
}

bool virtualKey(WORD vk, Key& out) {
    switch (vk) {
        case VK_RETURN: out = Key::Enter; return true;
        case VK_ESCAPE: out = Key::Escape; return true;
        case VK_TAB: out = Key::Tab; return true;
        case VK_BACK: out = Key::Backspace; return true;
        case VK_DELETE: out = Key::Delete; return true;
        case VK_INSERT: out = Key::Insert; return true;
        case VK_HOME: out = Key::Home; return true;
        case VK_END: out = Key::End; return true;
        case VK_PRIOR: out = Key::PageUp; return true;
        case VK_NEXT: out = Key::PageDown; return true;
        case VK_UP: out = Key::Up; return true;
        case VK_DOWN: out = Key::Down; return true;
        case VK_LEFT: out = Key::Left; return true;
        case VK_RIGHT: out = Key::Right; return true;
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        out = static_cast<Key>(static_cast<int>(Key::F1) + (vk - VK_F1));
        return true;
    }
    return false;
}

class WindowsTerminal final : public Terminal {
public:
    WindowsTerminal() {
        gSaved.in = GetStdHandle(STD_INPUT_HANDLE);
        gSaved.out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleMode(gSaved.in, &gSaved.inMode) || !GetConsoleMode(gSaved.out, &gSaved.outMode)) {
            throw std::runtime_error("interactive mode needs a console window (stdin/stdout are redirected)");
        }
        gSaved.inCodePage = GetConsoleCP();
        gSaved.outCodePage = GetConsoleOutputCP();

        const DWORD outMode = gSaved.outMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                              DISABLE_NEWLINE_AUTO_RETURN;
        if (!SetConsoleMode(gSaved.out, outMode)) {
            throw std::runtime_error("this console does not support ANSI escape sequences (Windows 10 or newer required)");
        }
        // No ENABLE_PROCESSED_INPUT (Ctrl+C arrives as a key) and no quick-edit (the mouse is ours).
        SetConsoleMode(gSaved.in, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        gActive = true;
        SetConsoleCtrlHandler(onConsoleControl, TRUE);
        write(kEnterSequence);
    }

    ~WindowsTerminal() override {
        restoreConsole();
        SetConsoleCtrlHandler(onConsoleControl, FALSE);
    }

    TerminalSize size() const override {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(gSaved.out, &info)) return {};
        return {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
    }

    void write(std::string_view bytes) override {
        if (bytes.empty()) return;
        const int wideLength = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        wide_.resize(static_cast<std::size_t>(wideLength));
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), wide_.data(), wideLength);
        // Large frames are written in chunks; never split an escape sequence across two calls,
        // since not every console implementation keeps parser state between writes.
        constexpr std::size_t kChunk = 16384;
        std::size_t begin = 0;
        const std::size_t total = static_cast<std::size_t>(wideLength);
        while (begin < total) {
            std::size_t end = std::min(total, begin + kChunk);
            if (end < total) {
                const std::size_t escape = wide_.rfind(L'\x1b', end);
                if (escape != std::wstring::npos && escape > begin) end = escape;
            }
            DWORD written = 0;
            if (!WriteConsoleW(gSaved.out, wide_.data() + begin, static_cast<DWORD>(end - begin), &written, nullptr) ||
                written == 0) {
                return;
            }
            begin += written;
        }
    }

    void poll(std::vector<Event>& out, int timeoutMs) override {
        if (WaitForSingleObject(gSaved.in, static_cast<DWORD>(timeoutMs < 0 ? INFINITE : timeoutMs)) != WAIT_OBJECT_0) return;

        DWORD available = 0;
        if (!GetNumberOfConsoleInputEvents(gSaved.in, &available) || available == 0) return;
        INPUT_RECORD records[128];
        DWORD read = 0;
        if (!ReadConsoleInputW(gSaved.in, records, 128, &read)) return;

        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(gSaved.out, &info);
        const SHORT windowTop = info.srWindow.Top;
        const SHORT windowLeft = info.srWindow.Left;

        for (DWORD i = 0; i < read; ++i) {
            const INPUT_RECORD& r = records[i];
            switch (r.EventType) {
                case KEY_EVENT: translateKey(r.Event.KeyEvent, out); break;
                case MOUSE_EVENT: translateMouse(r.Event.MouseEvent, windowLeft, windowTop, out); break;
                case WINDOW_BUFFER_SIZE_EVENT: {
                    const TerminalSize s = size();
                    out.push_back(ResizeEvent{s.columns, s.rows});
                    break;
                }
                default: break;
            }
        }
    }

private:
    void translateKey(const KEY_EVENT_RECORD& k, std::vector<Event>& out) {
        if (!k.bKeyDown) return;
        KeyEvent e;
        e.mods = modifiersFrom(k.dwControlKeyState);
        const wchar_t ch = k.uChar.UnicodeChar;
        const bool altGr = e.mods.ctrl && e.mods.alt && ch >= 0x20;  // AltGr produces printable text

        Key special;
        if (virtualKey(k.wVirtualKeyCode, special)) {
            e.key = special;
        } else if (e.mods.ctrl && !altGr && k.wVirtualKeyCode >= 'A' && k.wVirtualKeyCode <= 'Z') {
            e.key = Key::Char;
            e.ch = static_cast<char32_t>(std::towlower(static_cast<wint_t>(k.wVirtualKeyCode)));
        } else if (ch >= 0xd800 && ch <= 0xdbff) {
            highSurrogate_ = ch;
            return;
        } else if (ch >= 0xdc00 && ch <= 0xdfff) {
            e.key = Key::Char;
            e.ch = 0x10000 + ((static_cast<char32_t>(highSurrogate_) - 0xd800) << 10) + (static_cast<char32_t>(ch) - 0xdc00);
        } else if (ch >= 0x20) {
            e.key = Key::Char;
            e.ch = static_cast<char32_t>(ch);
        } else {
            return;  // lone modifier keys and the like
        }
        if (altGr) {
            e.mods.ctrl = false;
            e.mods.alt = false;
        }
        const WORD repeat = k.wRepeatCount > 0 ? k.wRepeatCount : 1;
        for (WORD i = 0; i < repeat; ++i) out.push_back(e);
    }

    void translateMouse(const MOUSE_EVENT_RECORD& m, SHORT left, SHORT top, std::vector<Event>& out) {
        MouseEvent e;
        e.x = m.dwMousePosition.X - left;
        e.y = m.dwMousePosition.Y - top;
        e.mods = modifiersFrom(m.dwControlKeyState);
        const DWORD buttons = m.dwButtonState & 0xffff;

        if (m.dwEventFlags & MOUSE_WHEELED) {
            const auto delta = static_cast<SHORT>(HIWORD(m.dwButtonState));
            e.action = delta > 0 ? MouseAction::WheelUp : MouseAction::WheelDown;
            out.push_back(e);
            return;
        }
        if (m.dwEventFlags & MOUSE_HWHEELED) return;

        struct Mapping {
            DWORD bit;
            MouseButton button;
        };
        static constexpr Mapping mapping[] = {{FROM_LEFT_1ST_BUTTON_PRESSED, MouseButton::Left},
                                              {RIGHTMOST_BUTTON_PRESSED, MouseButton::Right},
                                              {FROM_LEFT_2ND_BUTTON_PRESSED, MouseButton::Middle}};
        bool changed = false;
        for (const Mapping& mp : mapping) {
            const bool now = (buttons & mp.bit) != 0;
            const bool before = (buttons_ & mp.bit) != 0;
            if (now != before) {
                MouseEvent b = e;
                b.action = now ? MouseAction::Press : MouseAction::Release;
                b.button = mp.button;
                out.push_back(b);
                changed = true;
            }
        }
        buttons_ = buttons;
        if (!changed && (m.dwEventFlags & MOUSE_MOVED)) {
            e.action = MouseAction::Move;
            for (const Mapping& mp : mapping) {
                if (buttons & mp.bit) {
                    e.button = mp.button;
                    break;
                }
            }
            out.push_back(e);
        }
    }

    std::wstring wide_;
    DWORD buttons_ = 0;
    wchar_t highSurrogate_ = 0;
};

}  // namespace

std::unique_ptr<Terminal> Terminal::create() { return std::make_unique<WindowsTerminal>(); }

}  // namespace crt::term

#endif  // _WIN32
