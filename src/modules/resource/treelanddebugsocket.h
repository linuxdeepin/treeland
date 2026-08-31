// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Shared socket-naming logic for the treeland-debug remote-object channel.
//
// The socket name is a bare name (no path), so Qt's QLocalServer places it
// in QDir::tempPath() (typically /tmp on Linux, but respects $TMPDIR)
// regardless of XDG_RUNTIME_DIR.  This lets a treeland-debug client find the
// compositor's debug socket even when the two run under different runtime
// directories (e.g. global service as user dde vs. a normal user running
// treeland-debug).
//
// When the default name is already held by a live instance, a numeric suffix
// is chosen using a non-blocking advisory lock, mirroring libwayland's
// wl_display_add_socket_auto (see waylib/src/server/kernel/wsocket.cpp).
// Debug-compiled treeland uses a distinct base name so it can run alongside a
// release build; a debug-built treeland-debug client prefers the debug socket
// and falls back to the release socket.

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Provided by CMake as a compile definition (see top-level CMakeLists.txt).
#ifndef TREELAND_DEBUG_SOCKET
#  error "TREELAND_DEBUG_SOCKET must be provided by CMake (see top-level CMakeLists.txt)"
#endif

// Build-specific socket base name (bare name, no path, no "local:" scheme).
QString treelandDebugDefaultSocketName();

// Default URL for QRemoteObjectHost / client, e.g.
// "local:org.deepin.dde.treeland.debug".  Qt resolves the bare name to
// QDir::tempPath()/<name> (typically /tmp on Linux), so the socket is
// discoverable regardless of the caller's XDG_RUNTIME_DIR.
QString treelandDebugDefaultSocketUrl();

// Release-build socket base name (always defined so a debug-built client can
// fall back to a release instance).
#ifndef TREELAND_DEBUG_SOCKET_RELEASE
#  error "TREELAND_DEBUG_SOCKET_RELEASE must be provided by CMake (see top-level CMakeLists.txt)"
#endif
QString treelandDebugReleaseSocketName();

// Release socket URL, e.g. "local:org.deepin.dde.treeland.debug".
QString treelandDebugReleaseSocketUrl();

// Candidate socket URLs for the treeland-debug client, in preference order.
// A debug-built client prefers the debug instance and falls back to the
// release instance; a release-built client only uses the release instance.
// An explicit --url always overrides this list (handled by the caller).
QStringList treelandDebugCandidateSocketUrls();

// RAII lock that claims a local-socket name, mirroring wl_socket_lock() in
// libwayland.  An exclusive flock on "<name>.lock" (placed next to the
// socket by Qt, in QDir::tempPath()) is held for the lifetime of the
// QLocalServer, so two treeland instances can never silently steal each
// other's socket — QLocalServer::listen(), unlike raw bind(), replaces an
// existing socket file instead of failing with EADDRINUSE, so the flock is
// the real ownership guard.
//
// The destroy ordering matches wl_socket_destroy(): the lock file is unlinked
// *while* the fd (and thus the flock) is still held, so no other process can
// open the same inode and race the release.
class TreelandDebugSocketLock
{
public:
    TreelandDebugSocketLock() = default;
    ~TreelandDebugSocketLock();

    TreelandDebugSocketLock(const TreelandDebugSocketLock &) = delete;
    TreelandDebugSocketLock &operator=(const TreelandDebugSocketLock &) = delete;
    TreelandDebugSocketLock(TreelandDebugSocketLock &&other) noexcept;
    TreelandDebugSocketLock &operator=(TreelandDebugSocketLock &&other) noexcept;

    bool tryAcquire(const QString &socketName);
    void release();
    bool isHeld() const;

private:
    int m_fd = -1;
    QByteArray m_lockPath;
};

// Returns the first free socket name, Wayland-style: tries @p base, then
// "<base>-1", "<base>-2", ... up to "<base>-99".  For each candidate a lock
// file is acquired with flock(LOCK_EX|LOCK_NB); if the lock is already held
// by another process the name is in use and the next candidate is tried.
//
// On success @p lock holds the lock — the caller MUST keep it alive for the
// lifetime of the QLocalServer so the name stays claimed.  On failure (all
// names locked) returns an empty string and @p lock is released.
//
// The returned name is a bare name (no path); the caller passes it directly
// to QRemoteObjectHost as "local:<name>" so Qt resolves it consistently on
// both server and client.
QString treelandDebugPickFreeSocketName(
    TreelandDebugSocketLock &lock,
    const QString &base = treelandDebugDefaultSocketName());
