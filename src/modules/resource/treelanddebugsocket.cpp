// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelanddebugsocket.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace {

// Where the global service binds its debug socket: a directory of its own, NOT
// the process's XDG_RUNTIME_DIR.  Widening the runtime directory to 0711 makes
// QStandardPaths::RuntimeLocation reject it (Qt requires 0700) and fall back to
// "$TMPDIR/runtime-<user>", which is where sessions created during the window
// would then put their wayland sockets.  systemd creates this one
// (RuntimeDirectory= in misc/systemd/treeland.service.in) and recreates it 0700
// whenever treeland restarts, so nothing but the debug socket lives in it.
QString globalSocketDir()
{
    // Development and test hook: lets a run use a directory it can write, and
    // the caller who controls treeland's environment already controls treeland.
    const QString override = qEnvironmentVariable("TREELAND_DEBUG_DIR");
    if (!override.isEmpty())
        return QDir::cleanPath(override);
    return QStringLiteral("/run/treeland-debug");
}

// The runtime directory of this process, as QStandardPaths reports it
// ($XDG_RUNTIME_DIR): "/run/treeland" for the global service (see
// RuntimeDirectory= in misc/systemd/treeland.service.in) and
// "/run/user/<uid>" for a session.  Qt creates it with mode 0700 owned by the
// caller when missing and rejects one owned by somebody else, so the directory
// itself is the privacy boundary.  When the variable is unset or unusable Qt
// falls back to "$TMPDIR/runtime-<user>" -- that is /tmp again, which is what
// this socket must stay out of, so such a fallback counts as "no runtime
// directory".
QString runtimeSocketDir()
{
    const QString envDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
    if (envDir.isEmpty())
        return {};
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty() || !dir.startsWith(QLatin1Char('/')))
        return {};
    // Anything but the configured directory means QStandardPaths substituted
    // its temp-dir fallback.
    if (QDir::cleanPath(dir) != QDir::cleanPath(envDir))
        return {};
    return dir;
}

// "local:" + absolute socket path, the form both QRemoteObjectHost and a
// client outside the compositor's runtime directory can resolve.
QString socketUrl(const QString &dir, const QString &name)
{
    return QStringLiteral("local:%1/%2").arg(dir, name);
}

} // namespace

QString treelandDebugDefaultSocketName()
{
    return QStringLiteral(TREELAND_DEBUG_SOCKET);
}

QString treelandDebugGlobalSocketDir()
{
    return globalSocketDir();
}

QString treelandDebugSocketDir()
{
    // A session instance keeps its socket in its own runtime directory: it must
    // not bind in the global service's directory (not its own, likely absent or
    // not writable), and it never widens anything there anyway.
    if (qEnvironmentVariable("TREELAND_RUN_MODE") == QLatin1String("user"))
        return runtimeSocketDir();

    const QString globalDir = globalSocketDir();
    if (QFileInfo(globalDir).isDir())
        return globalDir;
    if (QDir().mkpath(globalDir)) {
        // mkpath() applies the umask, so the directory would be listable by
        // everyone; it must start as private as the runtime directory it
        // replaces.
        ::chmod(QFile::encodeName(globalDir).constData(), 0700);
        return globalDir;
    }
    // A hand-started instance that may not create the directory (anything but
    // root or dde): fall back to the runtime directory.  The no-auth window
    // stays unavailable there, because that directory is not Treeland's alone.
    return runtimeSocketDir();
}

QStringList treelandDebugSocketDirs()
{
    // A client knows only its own runtime directory, so it also probes the
    // global service's directories and -- when it may read them, which means
    // root -- every session directory under /run/user.  Entries are directories
    // that may or may not hold a socket; nothing is created or probed here.
    QStringList dirs;
    const QString own = treelandDebugSocketDir();
    if (!own.isEmpty())
        dirs << own;
    const QString globalDir = globalSocketDir();
    if (!dirs.contains(globalDir))
        dirs << globalDir;
    // Compositors from before the dedicated directory bound in their runtime
    // directory; keep probing it so a new client still finds one of those.
    const QString legacyGlobalDir = QStringLiteral("/run/treeland");
    if (!dirs.contains(legacyGlobalDir))
        dirs << legacyGlobalDir;
    const QDir sessions(QStringLiteral("/run/user"));
    const QStringList sessionIds = sessions.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                      QDir::Name);
    for (const QString &sessionId : sessionIds) {
        const QString dir = sessions.filePath(sessionId);
        if (!dirs.contains(dir))
            dirs << dir;
    }
    return dirs;
}

QString treelandDebugReleaseSocketName()
{
    return QStringLiteral(TREELAND_DEBUG_SOCKET_RELEASE);
}

QString treelandDebugSocketPath(const QString &socketName)
{
    // A bare name can only come from a hand-written --url: our own URLs always
    // carry the absolute path.  Qt resolves a bare server name against
    // QDir::tempPath() (QLocalServerPrivate::fullServerName), so mirror that
    // instead of inventing a second rule for the diagnostics below.
    if (socketName.startsWith(QLatin1Char('/')))
        return socketName;
    return QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + socketName;
}

QString treelandDebugSocketUrlFromEnv()
{
    const QString value = qEnvironmentVariable("TREELAND_DEBUG_SOCKET");
    if (value.isEmpty())
        return {};
    // A socket path or a bare socket name becomes a local: URL; a value that
    // already carries a scheme is taken as-is, so a remote host works too.
    if (value.contains(QLatin1Char(':')))
        return value;
    return QStringLiteral("local:") + value;
}

QStringList treelandDebugCandidateSocketUrls()
{
#ifdef TREELAND_DEBUG_DEV_BUILD
    const QStringList names{ treelandDebugDefaultSocketName(),
                             treelandDebugReleaseSocketName() };
#else
    const QStringList names{ treelandDebugDefaultSocketName() };
#endif
    const QStringList dirs = treelandDebugSocketDirs();

    QStringList urls;
    // Name-major: the debug instance in every directory is preferred over the
    // release instance, whichever directory each of them ended up using.
    for (const QString &name : names) {
        for (const QString &dir : dirs)
            urls << socketUrl(dir, name);
    }
    return urls;
}

// ── TreelandDebugSocketLock ──────────────────────────────────────────────

namespace {

// Returns true if a server is actively accepting connections on @p socketName.
// Used only after the flock is acquired, to avoid clobbering a live
// old-build instance that doesn't hold a lock file.  For local (AF_UNIX)
// sockets, QLocalSocket::connectToServer completes synchronously — a live
// server is in ConnectedState immediately, a stale/absent socket is in
// UnconnectedState with an error.  No waitForConnected() is needed, so this
// never blocks the compositor startup path.
bool socketIsLive(const QString &socketName)
{
    QLocalSocket probe;
    probe.connectToServer(socketName);
    return probe.state() == QLocalSocket::ConnectedState;
}

} // namespace

QString treelandDebugSocketPathForUrl(const QString &url)
{
    constexpr auto prefix = "local:";
    if (!url.startsWith(QLatin1String(prefix)))
        return {};
    const QString name = url.mid(static_cast<int>(qstrlen(prefix)));
    if (name.isEmpty())
        return {};
    // Same resolution Qt applies to a bare server name.
    return treelandDebugSocketPath(name);
}

namespace {

// QFileInfo has no socket test, and only a socket can be connected to.
bool isSocketPath(const QString &path)
{
    struct stat st {};
    return ::stat(QFile::encodeName(path).constData(), &st) == 0 && S_ISSOCK(st.st_mode);
}

// A process names itself whatever it likes (/proc/<pid>/comm, up to 15 bytes), so
// control bytes in that name would land on the terminal and let the process that
// owns the socket rewrite this diagnostic -- clear the screen, hide the real
// lines, spoof another program's message.  Keep the name readable instead.
QString printableProcessName(const QString &name)
{
    QString printable;
    printable.reserve(name.size());
    for (const QChar character : name.left(64)) {
        // C0, DEL and the C1 block: fromLatin1() maps the bytes 0x80-0x9f to
        // U+0080-U+009F, which terminals that decode C1 still act on.
        const ushort code = character.unicode();
        const bool control = code < 0x20 || (code >= 0x7f && code <= 0x9f);
        printable.append(control ? QLatin1Char('?') : character);
    }
    return printable;
}

} // namespace

QString treelandDebugSocketPeerText(const QLocalSocket &socket)
{
    struct ucred cred {};
    socklen_t length = sizeof(cred);
    const qintptr descriptor = socket.socketDescriptor();
    if (descriptor < 0
        || ::getsockopt(static_cast<int>(descriptor), SOL_SOCKET, SO_PEERCRED, &cred, &length) < 0) {
        return {};
    }

    const QString uid = QStringLiteral("uid %1").arg(cred.uid);
    QFile comm(QStringLiteral("/proc/%1/comm").arg(cred.pid));
    const QString name = comm.open(QIODevice::ReadOnly)
        ? printableProcessName(QString::fromLatin1(comm.readAll()).trimmed())
        : QString();
    return name.isEmpty() ? QStringLiteral("pid %1, %2").arg(cred.pid).arg(uid)
                          : QStringLiteral("pid %1 (%2), %3").arg(cred.pid).arg(name).arg(uid);
}

QStringList treelandDebugPublishedSocketPaths(const QString &url)
{
    const QString path = treelandDebugSocketPathForUrl(url);
    if (path.isEmpty())
        return {};

    QStringList paths;
    // Only real sockets: a plain file someone else dropped at the name is not a
    // published debug channel, and counting it would make the diagnostics name
    // it as one.
    if (isSocketPath(path))
        paths << path;

    // QLocalServer falls back to "<name>-<N>" when the base name is taken.
    // Only digits count: "<name>-dev" belongs to the other build type, and a
    // partial match such as "<name>-1.bak" is not a socket of this name.
    const QFileInfo info(path);
    const QDir dir(info.absolutePath());
    const QString prefix = info.fileName() + QLatin1Char('-');
    const QStringList siblings = dir.entryList({ prefix + QStringLiteral("[0-9]*") },
                                              QDir::Files | QDir::System | QDir::Hidden);
    for (const QString &sibling : siblings) {
        bool numbered = false;
        sibling.mid(prefix.size()).toUInt(&numbered);
        const QString siblingPath = dir.filePath(sibling);
        if (numbered && isSocketPath(siblingPath))
            paths << siblingPath;
    }
    return paths;
}

bool treelandDebugSocketPublished(const QString &url)
{
    return !treelandDebugPublishedSocketPaths(url).isEmpty();
}

QStringList treelandDebugExistingSocketUrls(const QStringList &urls)
{
    QStringList existing;
    for (const QString &url : urls) {
        const QString path = treelandDebugSocketPathForUrl(url);
        if (path.isEmpty() || QFileInfo::exists(path))
            existing << url;
    }
    return existing.isEmpty() ? urls : existing;
}

QStringList treelandDebugInstanceSocketUrls(const QStringList &candidateUrls)
{
    QStringList urls;
    for (const QString &candidate : candidateUrls) {
        // A candidate's own suffixed instances follow it, so the build-type
        // preference (debug before release) wins over the base name.
        QStringList group{ candidate };
        // A bare name in a "local:" URL resolves under QDir::tempPath(), so an
        // absolute candidate (local:/dir/name) must hand out absolute instance
        // names too -- otherwise the directory is dropped and the client probes a
        // same-named socket somewhere else.
        const bool absolute = candidate.startsWith(QStringLiteral("local:/"));
        // Bound what one candidate contributes: the directory is writable by its
        // owner, so a run of numbered sockets must not turn the privileged
        // client's discovery into a string of probes.
        constexpr int maxInstances = 8;
        int added = 0;
        for (const QString &path : treelandDebugPublishedSocketPaths(candidate)) {
            if (added == maxInstances)
                break;
            const QString url = QStringLiteral("local:")
                + (absolute ? path : QFileInfo(path).fileName());
            if (group.contains(url))
                continue;
            group << url;
            ++added;
        }
        for (const QString &url : std::as_const(group)) {
            if (!urls.contains(url))
                urls << url;
        }
    }
    return urls;
}

TreelandDebugSocketLock::~TreelandDebugSocketLock() { release(); }

TreelandDebugSocketLock::TreelandDebugSocketLock(TreelandDebugSocketLock &&other) noexcept
{
    std::swap(m_fd, other.m_fd);
    std::swap(m_lockPath, other.m_lockPath);
}

TreelandDebugSocketLock &TreelandDebugSocketLock::operator=(TreelandDebugSocketLock &&other) noexcept
{
    if (this != &other) {
        release();
        std::swap(m_fd, other.m_fd);
        std::swap(m_lockPath, other.m_lockPath);
    }
    return *this;
}

bool TreelandDebugSocketLock::tryAcquire(const QString &socketName)
{
    release();
    // Resolve the full path so the lock file sits next to the socket, and use
    // that path for every Qt call below: a bare name would be resolved by
    // QLocalServer/QLocalSocket against the process' temp dir, not the socket
    // directory.
    const QString fullPath = treelandDebugSocketPath(socketName);
    m_lockPath = QFile::encodeName(fullPath + QStringLiteral(".lock"));
    m_fd = ::open(m_lockPath.constData(), O_CREAT | O_CLOEXEC | O_RDWR,
                  S_IRUSR | S_IWUSR);
    if (m_fd < 0) {
        m_lockPath.clear();
        return false;
    }
    if (::flock(m_fd, LOCK_EX | LOCK_NB) < 0) {
        ::close(m_fd);
        m_fd = -1;
        m_lockPath.clear();
        return false;
    }
    // The flock rules out any other new-build treeland (they all use this
    // lock).  But a pre-existing old-build instance may be listening on this
    // socket without holding a lock file; reclaiming it would delete its
    // socket path and break its clients.  Probe liveness first and, if a live
    // server answers, leave the name to it and fail so the caller moves on to
    // a suffixed name.  A stale file left by a crash (no live server) is still
    // reclaimed.  For local (AF_UNIX) sockets the connect completes
    // synchronously, so this probe returns immediately.
    if (socketIsLive(fullPath)) {
        release();
        return false;
    }
    QLocalServer::removeServer(fullPath);
    return true;
}

void TreelandDebugSocketLock::release()
{
    // Unlink the lock file while the fd is still open (flock held), then
    // close — mirrors wl_socket_destroy() ordering.
    if (!m_lockPath.isEmpty()) {
        ::unlink(m_lockPath.constData());
        m_lockPath.clear();
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool TreelandDebugSocketLock::isHeld() const
{
    return m_fd >= 0;
}

QString treelandDebugPickFreeSocketName(
    TreelandDebugSocketLock &lock,
    const QString &base)
{
    // @p base is the absolute socket path from the caller; a bare name here
    // would be resolved by Qt against the temp dir, so it is not supported.
    if (!base.startsWith(QLatin1Char('/')))
        return {};

    lock.release();
    for (int i = 0; i < 100; ++i) {
        const QString candidate = (i == 0)
            ? base
            : QStringLiteral("%1-%2").arg(base).arg(i);
        if (lock.tryAcquire(candidate))
            return candidate;
    }
    return {};
}

// ── access policy ────────────────────────────────────────────────────────

namespace {

bool chmodPath(const QString &path, mode_t mode)
{
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

} // namespace

bool treelandDebugSetSocketAccess(const QString &socketPath, bool anyUser)
{
    if (socketPath.isEmpty())
        return false;

    const QString dir = QFileInfo(socketPath).absolutePath();
    // One directory may be opened and no other: the global service's own debug
    // directory, which holds nothing but this socket and is deliberately not the
    // process's XDG_RUNTIME_DIR, which Qt requires to stay 0700
    // (treelandDebugGlobalSocketDir()).  A session instance binds inside
    // /run/user/<uid> next to the session bus, so the comparison refuses it and
    // root stays required there; so does any hand-written --url.
    const QString globalDir = QFileInfo(treelandDebugGlobalSocketDir()).canonicalFilePath();
    const bool ownsDir = !globalDir.isEmpty()
        && QFileInfo(dir).canonicalFilePath() == globalDir;
    if (anyUser && !ownsDir)
        return false;

    // Only the socket is toggled on close.  The directory is widened once, when
    // a window first opens, and stays 0711 (traverse, not list) until the process
    // exits: the numeric-suffix naming exists because several compositor
    // instances can share one directory, so restoring 0700 here would cut off a
    // peer instance whose own window is still open.  Leaving it widened exposes
    // nothing -- the sockets are the gate, and every one of them is sealed (0000)
    // unless its own window is open.  systemd recreates the directory as 0700 on
    // the next start.
    bool ok = chmodPath(socketPath, anyUser ? 0666 : 0);
    if (ownsDir && anyUser)
        ok = chmodPath(dir, 0711) && ok;
    return ok;
}
