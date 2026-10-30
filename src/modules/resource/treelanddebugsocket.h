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

class QLocalSocket;

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

// Full filesystem path of a socket name, resolved exactly like
// QLocalServerPrivate::fullServerName: a bare name lives in QDir::tempPath()
// (typically /tmp), an absolute path is used as-is.  Needed to chmod() the
// socket after QRemoteObjectHost binds it.
QString treelandDebugSocketPath(const QString &socketName);

// Candidate socket URLs for the treeland-debug client, in preference order.
// A debug-built client prefers the debug instance and falls back to the
// release instance; a release-built client only uses the release instance.
// An explicit --url always overrides this list (handled by the caller).
QStringList treelandDebugCandidateSocketUrls();

// Full path of the local-socket file behind a "local:<name>" host URL, or an
// empty string when @p url is not a local socket URL.
QString treelandDebugSocketPathForUrl(const QString &url);

// Paths of the sockets published for @p url: the exact name when it exists, plus
// every "<name>-<N>" sibling (Qt appends a numeric suffix when the name is
// taken). Only Unix-domain sockets count -- a plain file that merely occupies
// the name is not a published channel. A differently built instance
// ("<name>-dev") is never included. More
// than one entry means several instances or a stale file left by a crash, which
// a caller reporting a connection failure should say verbatim instead of
// guessing. Empty when nothing is published.
QStringList treelandDebugPublishedSocketPaths(const QString &url);

// True when treelandDebugPublishedSocketPaths() finds a socket for @p url.
bool treelandDebugSocketPublished(const QString &url);

// Candidate URLs plus every instance published for them: each candidate is
// followed by its own "<name>-<N>" instances, so the candidate order (debug
// before release) still decides which build type is preferred.  A socket on the
// base name that belongs to someone else (or a stale file) must not make the
// client report a failure while an instance of this build is answering.
QStringList treelandDebugInstanceSocketUrls(const QStringList &candidateUrls);

// Describes the process on the other end of a connected @p socket, e.g.
// "pid 4711 (treeland), uid 1000", from the kernel's SO_PEERCRED: each side of a
// Unix socket knows its peer's credentials, so this works for another user's
// process without reading /proc.  The command name is omitted when
// /proc/<pid>/comm is unreadable (hidepid, restricted container).  Empty when the
// socket is not connected.  Used only on the connection-failure path, where
// naming the process replaces guessing whether the socket belongs to another
// instance.
QString treelandDebugSocketPeerText(const QLocalSocket &socket);

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
