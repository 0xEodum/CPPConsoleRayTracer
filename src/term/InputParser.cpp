#include "term/InputParser.hpp"

#include "term/Utf8.hpp"

namespace crt::term {
namespace {

constexpr char kEsc = '\x1b';
constexpr std::size_t kMaxSequenceLength = 64;

KeyEvent key(Key k, Modifiers mods = {}) {
    KeyEvent e;
    e.key = k;
    e.mods = mods;
    return e;
}

/// xterm encodes modifiers as 1 + bitmask(shift=1, alt=2, ctrl=4).
Modifiers decodeModifiers(int param) {
    Modifiers m;
    const int bits = param > 1 ? param - 1 : 0;
    m.shift = (bits & 1) != 0;
    m.alt = (bits & 2) != 0;
    m.ctrl = (bits & 4) != 0;
    return m;
}

std::vector<int> splitParams(std::string_view params) {
    std::vector<int> values;
    int current = 0;
    bool any = false;
    for (char c : params) {
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            any = true;
        } else if (c == ';' || c == ':') {
            values.push_back(any ? current : 0);
            current = 0;
            any = false;
        }
    }
    values.push_back(any ? current : 0);
    return values;
}

bool tildeKey(int code, Key& out) {
    switch (code) {
        case 1: case 7: out = Key::Home; return true;
        case 2: out = Key::Insert; return true;
        case 3: out = Key::Delete; return true;
        case 4: case 8: out = Key::End; return true;
        case 5: out = Key::PageUp; return true;
        case 6: out = Key::PageDown; return true;
        case 11: out = Key::F1; return true;
        case 12: out = Key::F2; return true;
        case 13: out = Key::F3; return true;
        case 14: out = Key::F4; return true;
        case 15: out = Key::F5; return true;
        case 17: out = Key::F6; return true;
        case 18: out = Key::F7; return true;
        case 19: out = Key::F8; return true;
        case 20: out = Key::F9; return true;
        case 21: out = Key::F10; return true;
        case 23: out = Key::F11; return true;
        case 24: out = Key::F12; return true;
        default: return false;
    }
}

bool letterKey(char c, Key& out) {
    switch (c) {
        case 'A': out = Key::Up; return true;
        case 'B': out = Key::Down; return true;
        case 'C': out = Key::Right; return true;
        case 'D': out = Key::Left; return true;
        case 'H': out = Key::Home; return true;
        case 'F': out = Key::End; return true;
        case 'P': out = Key::F1; return true;
        case 'Q': out = Key::F2; return true;
        case 'R': out = Key::F3; return true;
        case 'S': out = Key::F4; return true;
        default: return false;
    }
}

MouseEvent decodeMouse(int cb, int x, int y, bool release) {
    MouseEvent m;
    m.x = x;
    m.y = y;
    m.mods.shift = (cb & 4) != 0;
    m.mods.alt = (cb & 8) != 0;
    m.mods.ctrl = (cb & 16) != 0;
    const int bits = cb & 3;
    const MouseButton button = bits == 0 ? MouseButton::Left
                             : bits == 1 ? MouseButton::Middle
                             : bits == 2 ? MouseButton::Right
                                         : MouseButton::None;
    if (cb & 64) {
        m.action = (cb & 1) ? MouseAction::WheelDown : MouseAction::WheelUp;
    } else if (cb & 32) {
        m.action = MouseAction::Move;
        m.button = button;
    } else {
        m.action = release || bits == 3 ? MouseAction::Release : MouseAction::Press;
        m.button = button;
    }
    return m;
}

}  // namespace

void InputParser::parse(std::vector<Event>& out, bool flushEscape) {
    while (!buffer_.empty()) {
        std::size_t consumed = 0;
        Event event;
        const Result r = parseOne(consumed, event, flushEscape);
        if (r == Result::Incomplete) break;
        if (r == Result::Emitted) out.push_back(event);
        buffer_.erase(0, consumed > 0 ? consumed : 1);
    }
}

InputParser::Result InputParser::parseOne(std::size_t& consumed, Event& event, bool flushEscape) const {
    if (buffer_[0] != kEsc) return parsePlain(0, consumed, event);

    auto loneEscape = [&] {
        consumed = 1;
        event = key(Key::Escape);
        return Result::Emitted;
    };

    if (buffer_.size() == 1) return flushEscape ? loneEscape() : Result::Incomplete;

    Result r;
    switch (buffer_[1]) {
        case '[': r = parseCsi(consumed, event); break;
        case 'O': r = parseSs3(consumed, event); break;
        case kEsc: return loneEscape();
        default: {
            // ESC followed by a key = Alt+key.
            std::size_t inner = 0;
            r = parsePlain(1, inner, event);
            if (r == Result::Emitted) {
                if (auto* k = std::get_if<KeyEvent>(&event)) k->mods.alt = true;
                consumed = 1 + inner;
            } else if (r == Result::Skip) {
                consumed = 1 + inner;
            }
            break;
        }
    }
    if (r == Result::Incomplete && flushEscape) return loneEscape();
    return r;
}

InputParser::Result InputParser::parsePlain(std::size_t pos, std::size_t& consumed, Event& event) const {
    const auto c = static_cast<unsigned char>(buffer_[pos]);
    consumed = 1;
    if (c == '\r' || c == '\n') {
        event = key(Key::Enter);
        return Result::Emitted;
    }
    if (c == '\t') {
        event = key(Key::Tab);
        return Result::Emitted;
    }
    if (c == 0x7f || c == 0x08) {
        event = key(Key::Backspace);
        return Result::Emitted;
    }
    if (c >= 1 && c <= 26) {
        KeyEvent k = key(Key::Char);
        k.ch = static_cast<char32_t>('a' + c - 1);
        k.mods.ctrl = true;
        event = k;
        return Result::Emitted;
    }
    if (c < 0x20) return Result::Skip;

    const std::size_t len = utf8SequenceLength(c);
    if (pos + len > buffer_.size()) return Result::Incomplete;
    std::size_t p = pos;
    KeyEvent k = key(Key::Char);
    k.ch = decodeUtf8(buffer_, p);
    k.mods.shift = k.ch >= U'A' && k.ch <= U'Z';
    consumed = p - pos;
    event = k;
    return Result::Emitted;
}

InputParser::Result InputParser::parseCsi(std::size_t& consumed, Event& event) const {
    // Legacy X10 mouse: ESC [ M Cb Cx Cy (three raw bytes, offset by 32).
    if (buffer_.size() >= 3 && buffer_[2] == 'M') {
        if (buffer_.size() < 6) return Result::Incomplete;
        const int cb = static_cast<unsigned char>(buffer_[3]) - 32;
        const int x = static_cast<unsigned char>(buffer_[4]) - 33;
        const int y = static_cast<unsigned char>(buffer_[5]) - 33;
        consumed = 6;
        event = decodeMouse(cb, x, y, false);
        return Result::Emitted;
    }

    std::size_t i = 2;
    while (i < buffer_.size()) {
        const auto c = static_cast<unsigned char>(buffer_[i]);
        if (c >= 0x40 && c <= 0x7e) break;
        if (c < 0x20 || c > 0x3f) {  // not a parameter / intermediate byte: malformed
            consumed = i;
            return Result::Skip;
        }
        ++i;
    }
    if (i >= buffer_.size()) {
        if (buffer_.size() > kMaxSequenceLength) {
            consumed = buffer_.size();
            return Result::Skip;
        }
        return Result::Incomplete;
    }

    consumed = i + 1;
    const char final = buffer_[i];
    const std::string_view params(buffer_.data() + 2, i - 2);

    if (!params.empty() && params[0] == '<' && (final == 'M' || final == 'm')) {
        const auto v = splitParams(params.substr(1));
        if (v.size() < 3) return Result::Skip;
        event = decodeMouse(v[0], v[1] - 1, v[2] - 1, final == 'm');
        return Result::Emitted;
    }

    const auto v = splitParams(params);
    const Modifiers mods = decodeModifiers(v.size() >= 2 ? v[1] : 1);
    Key k;
    if (final == '~') {
        if (!tildeKey(v[0], k)) return Result::Skip;
        event = key(k, mods);
        return Result::Emitted;
    }
    if (final == 'Z') {
        Modifiers shifted = mods;
        shifted.shift = true;
        event = key(Key::Tab, shifted);
        return Result::Emitted;
    }
    if (letterKey(final, k)) {
        event = key(k, mods);
        return Result::Emitted;
    }
    return Result::Skip;  // focus reports, cursor position reports, ...
}

InputParser::Result InputParser::parseSs3(std::size_t& consumed, Event& event) const {
    if (buffer_.size() < 3) return Result::Incomplete;
    consumed = 3;
    Key k;
    if (buffer_[2] == 'M') {
        event = key(Key::Enter);
        return Result::Emitted;
    }
    if (!letterKey(buffer_[2], k)) return Result::Skip;
    event = key(k);
    return Result::Emitted;
}

}  // namespace crt::term
