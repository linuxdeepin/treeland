// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QStandardPaths>
#include <QString>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

// Default local-socket name of the treeland-debug remote-object host.
//
// The value is defined once in the top-level CMakeLists.txt as the
// TREELAND_DEBUG_SOCKET CMake variable and forwarded here as a compile
// definition of the same name, so the C++ default and the treeland-sd session
// unit (which substitutes @TREELAND_DEBUG_SOCKET@) can never drift apart.
// Debug builds use a distinct name so a debug treeland can run alongside a
// release one without colliding, and a debug-built treeland-debug client
// connects to the debug instance by default (WM-342).
#ifndef TREELAND_DEBUG_SOCKET
#  error "TREELAND_DEBUG_SOCKET must be provided by CMake (see top-level CMakeLists.txt)"
#endif
inline QString treelandDebugDefaultSocketName()
{
    return QStringLiteral(TREELAND_DEBUG_SOCKET);
}

inline QString treelandDebugDefaultSocketUrl()
{
    return QStringLiteral("local:") + treelandDebugDefaultSocketName();
}

// RAII lock file that claims a local-socket name, mirroring wl_socket_lock()
// in libwayland (src/wayland-server.c). An exclusive flock on
// "<runtime_dir>/<name>.lock" is held for the lifetime of the QLocalServer,
// so two treeland instances can never silently steal each other's socket —
// QLocalServer::listen(), unlike raw bind(), replaces an existing socket file
// instead of failing with EADDRINUSE, so the flock is the real ownership guard.
//
// The destroy ordering matches wl_socket_destroy(): the lock file is unlinked
// *while* the fd (and thus the flock) is still held, so no other process can
// open the same inode and race the release.
class TreelandDebugSocketLock
{
public:
    TreelandDebugSocketLock() = default;
    ~TreelandDebugSocketLock() { release(); }

    TreelandDebugSocketLock(const TreelandDebugSocketLock &) = delete;
    TreelandDebugSocketLock &operator=(const TreelandDebugSocketLock &) = delete;
    TreelandDebugSocketLock(TreelandDebugSocketLock &&other) noexcept
    {
        std::swap(m_fd, other.m_fd);
        std::swap(m_lockPath, other.m_lockPath);
    }
    TreelandDebugSocketLock &operator=(TreelandDebugSocketLock &&other) noexcept
    {
        if (this != &other) {
            release();
            std::swap(m_fd, other.m_fd);
            std::swap(m_lockPath, other.m_lockPath);
        }
        return *this;
    }

    // Tries to acquire the lock for @p socketName.
    // Returns true on success (lock held), false if another process owns it
    // or the lock file cannot be created.
    bool tryAcquire(const QString &socketName)
    {
        release();
        m_lockPath = lockFilePath(socketName);
        m_fd = open(m_lockPath.constData(), O_CREAT | O_CLOEXEC | O_RDWR,
                    S_IRUSR | S_IWUSR);
        if (m_fd < 0) {
            m_lockPath.clear();
            return false;
        }
        if (flock(m_fd, LOCK_EX | LOCK_NB) < 0) {
            close(m_fd);
            m_fd = -1;
            m_lockPath.clear();
            return false;
        }
        // Reclaim a stale socket file left by a crashed instance — same as
        // wl_socket_lock() unlinking the socket after acquiring the lock.
        QLocalServer::removeServer(socketName);
        return true;
    }

    void release()
    {
        // Unlink the lock file while the fd is still open (flock held), then
        // close — mirrors wl_socket_destroy() ordering.
        if (!m_lockPath.isEmpty()) {
            unlink(m_lockPath.constData());
            m_lockPath.clear();
        }
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
    }

    bool isHeld() const { return m_fd >= 0; }

private:
    // QLocalServer on Linux places the socket at $XDG_RUNTIME_DIR/<name>
    // (or /tmp/<name> as fallback). The lock file sits right next to it with
    // a ".lock" suffix, exactly like libwayland's LOCK_SUFFIX.
    static QByteArray lockFilePath(const QString &socketName)
    {
        QString path;
        if (socketName.startsWith(QLatin1Char('/')))
            path = socketName;
        else {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
            if (dir.isEmpty())
                dir = QStringLiteral("/tmp");
            path = QStringLiteral("%1/%2").arg(dir, socketName);
        }
        return QFile::encodeName(path + QStringLiteral(".lock"));
    }

    int m_fd = -1;
    QByteArray m_lockPath;
};

// Returns the first free socket name, Wayland-style: tries @p base, then
// "<base>-1", "<base>-2", ... up to "<base>-99". For each candidate a lock
// file is acquired with flock(LOCK_EX|LOCK_NB); if the lock is already held
// by another process the name is in use and the next candidate is tried.
//
// On success @p lock holds the lock — the caller MUST keep it alive for the
// lifetime of the QLocalServer so the name stays claimed. On failure (all
// names locked) returns an empty string and @p lock is released.
//
// Mirrors wl_display_add_socket_auto() (the do/while loop over "wayland-0",
// "wayland-1", …) combined with wl_socket_lock() (the flock-based ownership
// check) in libwayland's src/wayland-server.c.
inline QString treelandDebugPickFreeSocketName(
    TreelandDebugSocketLock &lock,
    const QString &base = treelandDebugDefaultSocketName())
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
