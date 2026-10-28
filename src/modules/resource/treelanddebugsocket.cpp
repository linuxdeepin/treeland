// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelanddebugsocket.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

QString treelandDebugDefaultSocketName()
{
    return QStringLiteral(TREELAND_DEBUG_SOCKET);
}

QString treelandDebugDefaultSocketUrl()
{
    return QStringLiteral("local:") + treelandDebugDefaultSocketName();
}

QString treelandDebugReleaseSocketName()
{
    return QStringLiteral(TREELAND_DEBUG_SOCKET_RELEASE);
}

QString treelandDebugReleaseSocketUrl()
{
    return QStringLiteral("local:") + treelandDebugReleaseSocketName();
}

QString treelandDebugSocketPath(const QString &socketName)
{
    if (socketName.startsWith(QLatin1Char('/')))
        return socketName;
    return QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + socketName;
}

QStringList treelandDebugCandidateSocketUrls()
{
#ifdef TREELAND_DEBUG_DEV_BUILD
    return { treelandDebugDefaultSocketUrl(), treelandDebugReleaseSocketUrl() };
#else
    return { treelandDebugDefaultSocketUrl() };
#endif
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

} // namespace

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
    // Resolve the full path Qt would use, so the lock file sits next to the
    // socket.  QLocalServerPrivate::fullServerName does the same.
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
    if (socketIsLive(socketName)) {
        release();
        return false;
    }
    QLocalServer::removeServer(socketName);
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
