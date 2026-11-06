// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Shared socket-naming logic for the treeland-debug remote-object channel.
//
// The socket lives in a directory of the compositor's own: the global service
// binds in /run/treeland-debug (see treelandDebugGlobalSocketDir()), a session
// in its per-user runtime directory.  Never in $XDG_RUNTIME_DIR for the global
// service, which must stay 0700 for Qt (QStandardPaths then falls back to
// $TMPDIR), and never in $TMPDIR itself: /tmp is world-writable, so any local
// user could squat the name, and a $TMPDIR-resolved path is invisible to a
// client whose TMPDIR differs.  A client probes every directory it can reach
// (see treelandDebugSocketDirs()), so it finds the compositor's socket even
// when the two run under different directories (global service as user dde
// vs. a normal user or root running treeland-debug).
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

// Directory of the global service's debug socket: a directory of its own
// (/run/treeland-debug, created by RuntimeDirectory= in
// misc/systemd/treeland.service.in), never the process's XDG_RUNTIME_DIR --
// Qt requires that one to stay 0700 and would move new sessions' wayland
// sockets to $TMPDIR if it were widened.  $TREELAND_DEBUG_DIR overrides it, for
// development and tests; the no-auth window still only opens this directory.
QString treelandDebugGlobalSocketDir();

// Directory this process binds its socket in: the global service's own
// treelandDebugGlobalSocketDir() (created 0700 if missing), and for a session
// (TREELAND_RUN_MODE=user) the runtime directory (QStandardPaths
// RuntimeLocation, "/run/user/<uid>").  Empty when neither is usable -- an
// unusable or "$TMPDIR/runtime-<user>" fallback -- which leaves the debug
// channel unpublished.
QString treelandDebugSocketDir();

// Candidate directories holding the debug socket, most-preferred first: this
// process' own directory, then the global service's one, then the runtime
// directory older global services used ("/run/treeland"), then every
// "/run/user/<uid>" session directory it can list (root only).  A client needs
// this because it cannot know the compositor's directory.
// Pure: no directory is created or probed here.
QStringList treelandDebugSocketDirs();

// Release-build socket base name (always defined so a debug-built client can
// fall back to a release instance).
#ifndef TREELAND_DEBUG_SOCKET_RELEASE
#  error "TREELAND_DEBUG_SOCKET_RELEASE must be provided by CMake (see top-level CMakeLists.txt)"
#endif
QString treelandDebugReleaseSocketName();

// Full filesystem path of a socket name, resolved exactly like
// QLocalServerPrivate::fullServerName: a bare name lives in QDir::tempPath(),
// an absolute path is used as-is.  Our own URLs always carry the absolute path,
// so this only decides what a hand-written `--url local:<name>` means -- for
// both the connection and the diagnostics, which is why it has to follow Qt.
QString treelandDebugSocketPath(const QString &socketName);

// Candidate socket URLs for the treeland-debug client, in preference order:
// every socket name below, in each directory from treelandDebugSocketDirs().
// A debug-built client prefers the debug instance and falls back to the
// release instance; a release-built client only uses the release instance.
// An explicit --url always overrides this list (handled by the caller).
QStringList treelandDebugCandidateSocketUrls();

// Full path of the local-socket file behind a "local:<name>" host URL, or an
// empty string when @p url is not a local socket URL.
QString treelandDebugSocketPathForUrl(const QString &url);

// Host URL from $TREELAND_DEBUG_SOCKET, or empty when the variable is unset.
// A plain socket path (or a bare socket name) is turned into a "local:" URL; a
// value that already carries a scheme is used verbatim.  The client applies it
// only when `--url` was not given, and only before falling back to its own
// probing -- so the priority is: command line, then this variable, then
// treelandDebugCandidateSocketUrls().
QString treelandDebugSocketUrlFromEnv();

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

// @p urls without the local candidates whose socket file is absent: connecting to
// one cannot succeed, so probing it would only spend a probe timeout.  A list
// whose every local candidate is absent is returned unchanged, so a caller's
// failure diagnostics can still name every place that was searched.
QStringList treelandDebugExistingSocketUrls(const QStringList &urls);

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

// Applies the socket's caller policy: with @p anyUser the debug socket becomes
// connectable by every local user, otherwise only root keeps access (mode 0000,
// so the kernel refuses every other connect()).  A socket is only reachable from
// inside a traversable directory, so the directory goes to 0711 too -- traverse
// but not list, which keeps the socket names unenumerable.  The directory is not
// narrowed again when @p anyUser is off: several instances can share it (see
// treelandDebugPickFreeSocketName), and one instance closing its window must not
// cut off a peer's still-open one.  The sockets are the access gate.
//
// Only one directory is ever widened: treelandDebugGlobalSocketDir(), which
// holds nothing but this socket.  A session instance binds in
// /run/user/<uid> next to the session bus, so it is refused before anything is
// touched and root stays required there; any other path is refused the same
// way, since widening a directory of somebody else's is exactly what this
// function must not do.  Returns false when the policy could not be applied; the
// socket stays usable for its owner either way, so callers only log it.
bool treelandDebugSetSocketAccess(const QString &socketPath, bool anyUser);

// RAII lock that claims a local-socket name, mirroring wl_socket_lock() in
// libwayland.  An exclusive flock on "<path>.lock" (placed next to the socket)
// is held for the lifetime of the
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

// Returns the first free socket, Wayland-style: tries @p base, then
// "<base>-1", "<base>-2", ... up to "<base>-99".  For each candidate a lock
// file is acquired with flock(LOCK_EX|LOCK_NB); if the lock is already held
// by another process the name is in use and the next candidate is tried.
//
// On success @p lock holds the lock — the caller MUST keep it alive for the
// lifetime of the QLocalServer so the name stays claimed.  On failure (all
// names locked) returns an empty string and @p lock is released.
//
// @p base is the absolute socket path to claim (treelandDebugSocketDir() plus
// the socket name) and the result is that path plus a numeric suffix: the
// caller turns it into "local:<path>" so server and client agree regardless of
// their own environment.
QString treelandDebugPickFreeSocketName(
    TreelandDebugSocketLock &lock,
    const QString &base);
