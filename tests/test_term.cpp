#include "TestFramework.hpp"
#include "term/Canvas.hpp"
#include "term/InputParser.hpp"
#include "term/Presenter.hpp"
#include "term/Utf8.hpp"
#include "ui/Widgets.hpp"

using namespace crt;
using namespace crt::term;

namespace {

std::vector<Event> parse(const std::string& bytes, bool flush = true) {
    InputParser parser;
    parser.feed(bytes);
    std::vector<Event> events;
    parser.parse(events, flush);
    return events;
}

const KeyEvent* key(const std::vector<Event>& events, std::size_t i = 0) {
    return i < events.size() ? std::get_if<KeyEvent>(&events[i]) : nullptr;
}

const MouseEvent* mouse(const std::vector<Event>& events, std::size_t i = 0) {
    return i < events.size() ? std::get_if<MouseEvent>(&events[i]) : nullptr;
}

}  // namespace

TEST_CASE("term: utf-8 encode/decode round trip") {
    for (char32_t cp : {U'a', U'é', U'▀', U'→', U'😀'}) {
        std::string s;
        appendUtf8(s, cp);
        std::size_t pos = 0;
        CHECK(decodeUtf8(s, pos) == cp);
        CHECK(pos == s.size());
    }
    CHECK(utf8Length("a▀→b") == 4);
    std::size_t pos = 0;
    CHECK(decodeUtf8("\xff", pos) == 0xfffd);
}

TEST_CASE("term: plain keys and control characters") {
    auto e = parse("aZ\r\t\x7f\x1a");
    REQUIRE(e.size() == 6);
    CHECK(key(e, 0)->is(U'a'));
    CHECK(key(e, 1)->ch == U'Z' && key(e, 1)->mods.shift);
    CHECK(key(e, 2)->key == Key::Enter);
    CHECK(key(e, 3)->key == Key::Tab);
    CHECK(key(e, 4)->key == Key::Backspace);
    CHECK(key(e, 5)->isCtrl(U'z'));
    auto u = parse("\xd0\xaf");  // Cyrillic capital Ya
    REQUIRE(u.size() == 1);
    CHECK(key(u)->ch == 0x42f);
}

TEST_CASE("term: escape sequences") {
    auto e = parse("\x1b[A\x1b[1;5C\x1bOP\x1b[15~\x1b[3~\x1b[Z\x1b[5;2~");
    REQUIRE(e.size() == 7);
    CHECK(key(e, 0)->key == Key::Up);
    CHECK(key(e, 1)->key == Key::Right && key(e, 1)->mods.ctrl);
    CHECK(key(e, 2)->key == Key::F1);
    CHECK(key(e, 3)->key == Key::F5);
    CHECK(key(e, 4)->key == Key::Delete);
    CHECK(key(e, 5)->key == Key::Tab && key(e, 5)->mods.shift);
    CHECK(key(e, 6)->key == Key::PageUp && key(e, 6)->mods.shift);
    auto alt = parse("\x1bx");
    REQUIRE(alt.size() == 1);
    CHECK(key(alt)->ch == U'x' && key(alt)->mods.alt);
}

TEST_CASE("term: lone escape waits for more input unless flushed") {
    InputParser parser;
    std::vector<Event> events;
    parser.feed("\x1b");
    parser.parse(events, false);
    CHECK(events.empty());
    CHECK(parser.hasPending());
    parser.feed("[B");  // the rest of the sequence arrives later
    parser.parse(events, false);
    REQUIRE(events.size() == 1);
    CHECK(key(events)->key == Key::Down);

    events.clear();
    parser.feed("\x1b");
    parser.parse(events, true);
    REQUIRE(events.size() == 1);
    CHECK(key(events)->key == Key::Escape);
    CHECK(!parser.hasPending());
}

TEST_CASE("term: SGR mouse reports") {
    auto e = parse("\x1b[<0;10;5M\x1b[<32;11;6M\x1b[<0;11;6m\x1b[<2;1;1M\x1b[<64;3;3M\x1b[<65;3;3M\x1b[<35;7;8M\x1b[<4;2;2M");
    REQUIRE(e.size() == 8);
    CHECK(mouse(e, 0)->action == MouseAction::Press && mouse(e, 0)->button == MouseButton::Left);
    CHECK(mouse(e, 0)->x == 9 && mouse(e, 0)->y == 4);
    CHECK(mouse(e, 1)->action == MouseAction::Move && mouse(e, 1)->button == MouseButton::Left);
    CHECK(mouse(e, 2)->action == MouseAction::Release);
    CHECK(mouse(e, 3)->button == MouseButton::Right);
    CHECK(mouse(e, 4)->action == MouseAction::WheelUp);
    CHECK(mouse(e, 5)->action == MouseAction::WheelDown);
    CHECK(mouse(e, 6)->action == MouseAction::Move && mouse(e, 6)->button == MouseButton::None);
    CHECK(mouse(e, 7)->mods.shift);
}

TEST_CASE("term: malformed input does not stall the parser") {
    auto e = parse("\x1b[99;99Xq\x1b[?1;2c");
    REQUIRE(e.size() == 1);
    CHECK(key(e)->is(U'q'));
}

TEST_CASE("term: canvas text, frames and clipping") {
    Canvas c(10, 4);
    CHECK(c.text(8, 1, "hello", {255, 0, 0}) == 2);
    CHECK(c.at(8, 1).ch == U'h' && c.at(9, 1).ch == U'e');
    c.frame({0, 0, 10, 4}, {1, 2, 3}, {4, 5, 6}, "T");
    CHECK(c.at(0, 0).ch == U'┌' && c.at(9, 3).ch == U'┘');
    CHECK(c.at(1, 1).bg == (Rgb8{4, 5, 6}));
    c.text(-3, 2, "abcd", {9, 9, 9});
    CHECK(c.at(0, 2).ch == U'd');
}

TEST_CASE("term: half-block image mapping") {
    Image8 img(2, 3);
    img.at(0, 0) = {255, 0, 0};
    img.at(0, 1) = {0, 0, 255};
    img.at(1, 0) = {9, 9, 9};
    img.at(1, 1) = {9, 9, 9};
    img.at(0, 2) = {1, 2, 3};
    Canvas c(2, 2);
    c.blitImage(img, 0, 0, ImageStyle::HalfBlock);
    CHECK(c.at(0, 0).ch == U'▀');
    CHECK(c.at(0, 0).fg == (Rgb8{255, 0, 0}));
    CHECK(c.at(0, 0).bg == (Rgb8{0, 0, 255}));
    CHECK(c.at(1, 0).ch == U' ');  // uniform cell collapses to a blank
    CHECK(c.at(0, 1).fg == (Rgb8{1, 2, 3}));  // odd height: last row repeats
    CHECK(imageRowsFor(ImageStyle::HalfBlock, 10) == 20);
    CHECK(imageRowsFor(ImageStyle::Ascii, 10) == 10);
}

TEST_CASE("term: presenter only sends what changed") {
    Canvas c(20, 5);
    c.clear({0, 0, 0});
    c.text(2, 2, "hi", {255, 255, 255});
    Presenter p;
    const std::string first = p.render(c);
    CHECK(first.find("\x1b[2J") != std::string::npos);
    CHECK(first.find("hi") != std::string::npos);
    CHECK(p.render(c).empty());

    c.text(3, 2, "o", {255, 255, 255});
    const std::string delta = p.render(c);
    CHECK(!delta.empty());
    CHECK(delta.find("\x1b[3;4H") != std::string::npos);  // one-based row;col of the changed cell
    CHECK(delta.find("hi") == std::string::npos);  // unchanged cells are not re-sent
    CHECK(delta.find('o') != std::string::npos);
    CHECK(delta.size() < 80);

    p.setTolerance(4);
    c.at(0, 0).bg = {3, 3, 3};  // below the tolerance: not worth sending
    CHECK(p.render(c).empty());
    p.invalidate();
    CHECK(p.render(c).find("\x1b[2J") != std::string::npos);
}

TEST_CASE("term: 256-colour palette mapping") {
    CHECK(Presenter::paletteIndex({0, 0, 0}) == 16);
    CHECK(Presenter::paletteIndex({255, 0, 0}) == 196);
    CHECK(Presenter::paletteIndex({255, 255, 255}) == 231);
    CHECK(Presenter::paletteIndex({128, 128, 128}) == 244);
    Presenter p(ColorMode::Palette256);
    Canvas c(3, 1);
    c.text(0, 0, "x", {255, 0, 0});
    CHECK(p.render(c).find("38;5;196") != std::string::npos);
}

TEST_CASE("ui: word wrapping") {
    const auto lines = ui::wrapText("the quick brown fox jumps over the lazy dog", 10);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0] == "the quick");
    CHECK(lines[4] == "dog");
    for (const auto& l : lines) CHECK(utf8Length(l) <= 10);
    CHECK(ui::formatCount(1234567.0) == "1.23M");
    CHECK(ui::formatCount(950.0) == "950");
}
