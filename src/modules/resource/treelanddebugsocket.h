// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Shared socket-naming logic for the treeland-debug remote-object channel.
//
// The socket lives in the compositor's runtime directory, i.e.
// $XDG_RUNTIME_DIR as reported by QStandardPaths::RuntimeLocation:
// /run/treeland for the global service, /run/user/<uid> for a session.  Never
// in $TMPDIR: /tmp is world-writable, so any local user could squat the name,
// and a $TMPDIR-resolved path is invisible to a client whose TMPDIR differs.
// A client probes the runtime directories it can reach (see
// treelandDebugSocketDirs()), so it finds the compositor's socket even when
// the two run under different runtime directories (global service as user dde
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

// Provided by CMake as a compile definition (see top-level CMakeLists.txt).
#ifndef TREELAND_DEBUG_SOCKET
#  error "TREELAND_DEBUG_SOCKET must be provided by CMake (see top-level CMakeLists.txt)"
#endif

// Build-specific socket base name (bare name, no path, no "local:" scheme).
QString treelandDebugDefaultSocketName();

// Runtime directory of this process (QStandardPaths::RuntimeLocation), where
// the compositor binds its socket and where the qtests below expect clients to
// look first: "/run/treeland" for the global service, "/run/user/<uid>" for a
// session.  Qt creates it (0700, owned by the caller) when missing and rejects
// one owned by another user; an unusable or "$TMPDIR/runtime-<user>" fallback
// yields an empty string, which leaves the debug channel unpublished.
QString treelandDebugSocketDir();

// Candidate directories holding the debug socket, most-preferred first: this
// process' runtime directory, then the global service's "/run/treeland", then
// every "/run/user/<uid>" session directory it can list (root only).  A client
// needs this because it cannot know the compositor's runtime directory.
// Pure: neither directory is created or probed here.
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
