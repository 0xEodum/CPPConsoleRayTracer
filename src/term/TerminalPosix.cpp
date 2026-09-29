#ifndef _WIN32

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <stdexcept>

#include "term/InputParser.hpp"
#include "term/Terminal.hpp"

namespace crt::term {
namespace {

// Alternate screen, hidden cursor, no auto-wrap, mouse: clicks + drags + motion, SGR encoding.
constexpr char kEnterSequence[] = "\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?1000h\x1b[?1002h\x1b[?1003h\x1b[?1006h\x1b[2J";
constexpr char kLeaveSequence[] = "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";

termios gOriginalMode{};
std::atomic<bool> gActive{false};
volatile std::sig_atomic_t gResized = 0;

/// Async-signal-safe: only write(2) and tcsetattr(3) are used.
void restoreTerminal() {
    if (!gActive.exchange(false)) return;
    const ssize_t ignored = ::write(STDOUT_FILENO, kLeaveSequence, sizeof(kLeaveSequence) - 1);
    (void)ignored;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOriginalMode);
}

void onFatalSignal(int sig) {
    restoreTerminal();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void onResize(int) { gResized = 1; }

void installHandler(int sig, void (*handler)(int)) {
    struct sigaction action {};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;  // no SA_RESTART: a resize must interrupt poll()
    sigaction(sig, &action, nullptr);
}

class PosixTerminal final : public Terminal {
public:
    PosixTerminal() {
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            throw std::runtime_error("interactive mode needs a terminal (stdin/stdout are redirected)");
        }
        if (tcgetattr(STDIN_FILENO, &gOriginalMode) != 0) throw std::runtime_error("tcgetattr failed");

        termios raw = gOriginalMode;
        raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
        raw.c_cflag |= CS8;
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) throw std::runtime_error("tcsetattr failed");
        gActive = true;

        for (int sig : {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL}) {
            installHandler(sig, onFatalSignal);
        }
        installHandler(SIGWINCH, onResize);
        write(kEnterSequence);
    }

    ~PosixTerminal() override {
        restoreTerminal();
        std::signal(SIGWINCH, SIG_DFL);
    }

    TerminalSize size() const override {
        winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
            return {ws.ws_col, ws.ws_row};
        }
        return {};
    }

    void write(std::string_view bytes) override {
        while (!bytes.empty()) {
            const ssize_t n = ::write(STDOUT_FILENO, bytes.data(), bytes.size());
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                return;
            }
            bytes.remove_prefix(static_cast<std::size_t>(n));
        }
    }

    void poll(std::vector<Event>& out, int timeoutMs) override {
        const std::size_t before = out.size();
        checkResize(out);
        if (out.size() == before) readInput(timeoutMs);
        parser_.parse(out, false);

        // A trailing ESC is either the Escape key or the start of a sequence still in flight:
        // give the rest of it a moment to arrive before deciding.
        if (parser_.hasPending()) {
            if (readInput(25)) parser_.parse(out, false);
            if (parser_.hasPending()) parser_.parse(out, true);
        }
        checkResize(out);
    }

private:
    void checkResize(std::vector<Event>& out) const {
        if (gResized != 0) {
            gResized = 0;
            const TerminalSize s = size();
            out.push_back(ResizeEvent{s.columns, s.rows});
        }
    }

    bool readInput(int timeoutMs) {
        pollfd pfd{STDIN_FILENO, POLLIN, 0};
        const int ready = ::poll(&pfd, 1, timeoutMs);
        if (ready <= 0 || !(pfd.revents & POLLIN)) return false;
        bool any = false;
        char buffer[4096];
        for (;;) {
            const ssize_t n = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (n <= 0) break;
            parser_.feed({buffer, static_cast<std::size_t>(n)});
            any = true;
            if (static_cast<std::size_t>(n) < sizeof(buffer)) break;
        }
        return any;
    }

    InputParser parser_;
};

}  // namespace

std::unique_ptr<Terminal> Terminal::create() { return std::make_unique<PosixTerminal>(); }

}  // namespace crt::term

#endif  // !_WIN32
