// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellline.h"

#include "debughelpers.h"

#include <QByteArray>

#include <cerrno>
#include <csignal>
#include <iterator>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

namespace {

// Enough to recall what was typed a while ago without keeping every line of a
// long session alive.
constexpr qsizetype kMaxHistory = 500;

// One keypress, with escape sequences folded into a single key so the caller
// never has to look at the terminal protocol.
enum class Key {
    Text,
    Enter,
    Backspace,
    Tab,
    Interrupt,
    Eof,
    StdinClosed,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    Delete,
    KillLine,
    KillToEnd,
};

struct Keypress
{
    Key key = Key::Text;
    QString text;
};

void writeRaw(const QByteArray &bytes)
{
    const char *data = bytes.constData();
    qsizetype left = bytes.size();
    while (left > 0) {
        const ssize_t written = ::write(STDOUT_FILENO, data, size_t(left));
        if (written <= 0)
            return;
        data += written;
        left -= written;
    }
}

// A byte read while looking for the rest of an escape sequence that turned out
// not to be one; it belongs to the next keypress, so hand it back instead of
// dropping it (Esc followed by Enter must still be an Enter).
int g_pendingByte = -1;

int readByte()
{
    if (g_pendingByte >= 0) {
        const int c = g_pendingByte;
        g_pendingByte = -1;
        return c;
    }
    unsigned char c = 0;
    return ::read(STDIN_FILENO, &c, 1) == 1 ? int(c) : -1;
}

// How long to wait for the rest of an escape sequence before deciding the Esc
// was pressed on its own; terminals send the whole sequence in one burst.
constexpr int kEscapeTimeoutMs = 30;

int readByteTimeout(int timeoutMs)
{
    if (g_pendingByte >= 0)
        return readByte();
    struct pollfd fd {};
    fd.fd = STDIN_FILENO;
    fd.events = POLLIN;
    int ready = 0;
    do {
        ready = ::poll(&fd, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);
    return ready > 0 ? readByte() : -1;
}

// Columns @p text occupies on the terminal, so the cursor can be put back after
// a redraw. wcwidth() answers -1 outside a UTF-8 locale and every command
// argument is an id, a key name or a path, so one column per unknown character
// is close enough.
// shortcut: a wide CJK character counts as one column in the C locale.
int displayWidth(const QString &text)
{
    int columns = 0;
    for (const char32_t code : text.toUcs4()) {
        const int width = ::wcwidth(wchar_t(code));
        columns += width > 0 ? width : 1;
    }
    return columns;
}

Keypress readEscape()
{
    const int next = readByteTimeout(kEscapeTimeoutMs);
    if (next < 0)
        return {}; // a lone Esc, or a partial sequence that timed out
    // SS3 (ESC O A) is the same arrow when the terminal is in application
    // cursor mode; treeland-debug never sets smkx, but a multiplexer might.
    if (next == 'O') {
        switch (readByte()) {
        case 'A': return { Key::Up, {} };
        case 'B': return { Key::Down, {} };
        case 'C': return { Key::Right, {} };
        case 'D': return { Key::Left, {} };
        case 'H': return { Key::Home, {} };
        case 'F': return { Key::End, {} };
        default: return {};
        }
    }
    if (next != '[') {
        g_pendingByte = next;
        return {}; // a lone Esc
    }
    QByteArray params;
    int c = 0;
    while (true) {
        c = readByte();
        if (c < 0)
            return {};
        if (c >= '@' && c <= '~')
            break;
        params.append(char(c));
    }
    switch (c) {
    case 'A': return { Key::Up, {} };
    case 'B': return { Key::Down, {} };
    case 'C': return { Key::Right, {} };
    case 'D': return { Key::Left, {} };
    case 'H': return { Key::Home, {} };
    case 'F': return { Key::End, {} };
    case '~':
        if (params == "1" || params == "7")
            return { Key::Home, {} };
        if (params == "4" || params == "8")
            return { Key::End, {} };
        if (params == "3")
            return { Key::Delete, {} };
        return {};
    default:
        return {};
    }
}

Keypress readKey()
{
    const int c = readByte();
    if (c < 0)
        return { Key::StdinClosed, {} };
    switch (c) {
    case '\r':
    case '\n': return { Key::Enter, {} };
    case 0x7f:
    case 0x08: return { Key::Backspace, {} };
    case '\t': return { Key::Tab, {} };
    case 0x03: return { Key::Interrupt, {} };
    case 0x04: return { Key::Eof, {} };
    case 0x01: return { Key::Home, {} };
    case 0x05: return { Key::End, {} };
    case 0x0b: return { Key::KillToEnd, {} };
    case 0x15: return { Key::KillLine, {} };
    case 0x1b: return readEscape();
    default: break;
    }
    if (c < 0x20)
        return {}; // a control key with no binding here: ignore it

    // A UTF-8 lead byte says how many continuation bytes belong to the
    // character; they are already in the input buffer, so read them now.
    const int extra = c < 0x80 ? 0
        : (c >> 5) == 0x6      ? 1
        : (c >> 4) == 0xe      ? 2
        : (c >> 3) == 0x1e     ? 3
                               : 0;
    QByteArray bytes(1, char(c));
    for (int i = 0; i < extra; ++i) {
        const int next = readByte();
        if (next < 0)
            break;
        bytes.append(char(next));
    }
    return { Key::Text, QString::fromUtf8(bytes) };
}

// Completes the word before the cursor, printing the candidates when nothing
// new can be inserted.
void completeWord(QString *line, int *cursor)
{
    const CompletionEdit edit = completeLine(*line, *cursor);
    if (!edit.list.isEmpty())
        writeRaw("\r\n" + edit.list.join(QStringLiteral("  ")).toUtf8() + "\r\n");
    *line = edit.line;
    *cursor = edit.cursor;
}

// A signal that ends the process while the terminal is raw must put it back
// first, or the shell that inherits it is left unusable. tcsetattr() is
// async-signal-safe; the default action is re-raised afterwards so the exit
// status still reports the signal.
struct termios g_savedTermios {};
volatile sig_atomic_t g_rawMode = 0;
constexpr int kGuardedSignals[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT };
constexpr int kGuardedSignalCount = int(std::size(kGuardedSignals));

void restoreTerminalOnSignal(int signo)
{
    if (g_rawMode) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &g_savedTermios);
        g_rawMode = 0;
    }
    ::signal(signo, SIG_DFL);
    ::raise(signo);
}

// Puts the terminal back into the mode it was saved in.
class RawMode
{
public:
    bool enter()
    {
        if (::tcgetattr(STDIN_FILENO, &m_saved) != 0)
            return false;
        struct termios raw = m_saved;
        // ISIG goes too, so Ctrl+C arrives as a keypress the prompt can turn
        // into "drop this line"; a live view gets SIGINT after the terminal is
        // restored. The cost is that Ctrl+Z and Ctrl+\ do nothing at the prompt.
        raw.c_lflag &= ~tcflag_t(ICANON | ECHO | ISIG | IEXTEN);
        // Ctrl+S must not freeze the prompt and '^M' must stay an Enter.
        raw.c_iflag &= ~tcflag_t(IXON | ICRNL);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        // TCSADRAIN, not TCSAFLUSH: a line typed ahead of the prompt is input,
        // not garbage, and this runs again for every line.
        if (::tcsetattr(STDIN_FILENO, TCSADRAIN, &raw) != 0)
            return false;
        m_valid = true;
        g_savedTermios = m_saved;
        g_rawMode = 1;
        installGuards();
        return true;
    }

    ~RawMode() { restore(); }

private:
    void installGuards()
    {
        struct sigaction action {};
        action.sa_handler = &restoreTerminalOnSignal;
        sigemptyset(&action.sa_mask);
        for (int i = 0; i < kGuardedSignalCount; ++i)
            ::sigaction(kGuardedSignals[i], &action, &m_oldActions[i]);
    }

    void restore()
    {
        if (!m_valid) // tcgetattr() failed, so there is nothing to put back
            return;
        ::tcsetattr(STDIN_FILENO, TCSADRAIN, &m_saved);
        g_rawMode = 0;
        for (int i = 0; i < kGuardedSignalCount; ++i)
            ::sigaction(kGuardedSignals[i], &m_oldActions[i], nullptr);
        m_valid = false;
    }

    struct termios m_saved {};
    struct sigaction m_oldActions[kGuardedSignalCount] {};
    bool m_valid = false;
};

} // namespace

bool ShellLineEditor::readLine(const QString &prompt, QString *line)
{
    RawMode raw;
    if (!raw.enter())
        return false;

    QString text;
    // shortcut: the cursor steps by QChar, so it can land between the halves of
    // a surrogate pair; treeland-debug's arguments are ASCII, so this is not
    // worth tracking code points for.
    int cursor = 0;
    qsizetype historyPos = m_history.size(); // one past the end: the new line
    QString draft;                          // the line being edited when browsing history
    bool eof = false;
    bool done = false;

    const auto redraw = [&]() {
        QByteArray bytes = "\r" + prompt.toUtf8() + text.toUtf8() + "\x1b[K";
        const int back = displayWidth(text.mid(cursor));
        if (back > 0)
            bytes += "\x1b[" + QByteArray::number(back) + 'D';
        writeRaw(bytes);
    };

    while (!done) {
        redraw();
        const Keypress key = readKey();
        switch (key.key) {
        case Key::Text:
            if (!key.text.isEmpty()) {
                text.insert(cursor, key.text);
                cursor += int(key.text.size());
            }
            break;
        case Key::Enter:
            if (!text.trimmed().isEmpty()
                && (m_history.isEmpty() || m_history.last() != text)) {
                m_history.append(text);
                if (m_history.size() > kMaxHistory)
                    m_history.removeFirst();
            }
            writeRaw("\r\n");
            done = true;
            break;
        case Key::Backspace:
            if (cursor > 0) {
                text.remove(cursor - 1, 1);
                --cursor;
            }
            break;
        case Key::Delete:
            if (cursor < text.size())
                text.remove(cursor, 1);
            break;
        case Key::Tab:
            completeWord(&text, &cursor);
            break;
        case Key::Interrupt:
            // A live view is interrupted by SIGINT (ISIG is back on by then);
            // at the prompt Ctrl+C just drops what was typed.
            writeRaw("^C\r\n");
            text.clear();
            draft.clear();
            cursor = 0;
            historyPos = m_history.size();
            break;
        case Key::Eof: // Ctrl+D
            if (text.isEmpty()) {
                writeRaw("\r\n");
                eof = true;
                done = true;
            } else if (cursor < text.size()) {
                text.remove(cursor, 1); // like readline: delete under the cursor
            }
            break;
        case Key::StdinClosed:
            // stdin is gone: end the session whatever the line holds, instead
            // of deleting a character and reading -1 again in a redraw loop.
            writeRaw("\r\n");
            eof = true;
            done = true;
            break;
        case Key::Left:
            if (cursor > 0)
                --cursor;
            break;
        case Key::Right:
            if (cursor < text.size())
                ++cursor;
            break;
        case Key::Home:
            cursor = 0;
            break;
        case Key::End:
            cursor = int(text.size());
            break;
        case Key::KillLine:
            text.remove(0, cursor);
            cursor = 0;
            break;
        case Key::KillToEnd:
            text.truncate(cursor);
            break;
        case Key::Up:
            if (historyPos > 0) {
                if (historyPos == m_history.size())
                    draft = text;
                --historyPos;
                text = m_history.at(historyPos);
                cursor = int(text.size());
            }
            break;
        case Key::Down:
            if (historyPos < m_history.size()) {
                ++historyPos;
                text = historyPos == m_history.size() ? draft : m_history.at(historyPos);
                cursor = int(text.size());
            }
            break;
        }
    }

    *line = text;
    return !eof;
}
