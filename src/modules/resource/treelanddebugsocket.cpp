// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelanddebugsocket.h"

#include <QDir>
#include <QFile>
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

// Mirrors QLocalServerPrivate::fullServerName: bare names are resolved
// to <tempDir>/<name>, where tempDir = QDir::tempPath() (respects $TMPDIR,
// defaults to /tmp on Linux).  Absolute paths are used as-is.  Must match
// Qt exactly so the lock file sits next to the socket.
QString resolveFullServerName(const QString &name)
{
    if (name.startsWith(QLatin1Char('/')))
        return name;
    return QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + name;
}

} // namespace

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
    const QString fullPath = resolveFullServerName(socketName);
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
