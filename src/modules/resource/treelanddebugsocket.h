// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

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

// Returns the first free local-socket name, Wayland-style: @p base, or
// "<base>-1", "<base>-2", ... when a live server already owns it.
//
// This is a *pre-bind* collision check: QLocalServer/QRemoteObjectHost silently
// steal an in-use name (both listen() calls succeed), so liveness has to be
// probed explicitly with a tentative connection. A stale socket file left by a
// crashed instance is reclaimed so the default name survives a restart.
// ponytail: probe-then-bind has a tiny TOCTOU window if two treeland instances
// start at the same instant; acceptable for a debug channel, upgrade with a
// bind-and-verify loop if it ever matters.
inline QString treelandDebugPickFreeSocketName(const QString &base = treelandDebugDefaultSocketName())
{
    QString candidate = base;
    for (int i = 1; i <= 100; ++i) {
        QLocalSocket probe;
        probe.connectToServer(candidate);
        if (!probe.waitForConnected(100)) {
            if (probe.error() == QLocalSocket::ConnectionRefusedError)
                QLocalServer::removeServer(candidate); // stale file from a crash
            return candidate; // free (ServerNotFoundError) or reclaimed
        }
        probe.disconnectFromServer();
        candidate = QStringLiteral("%1-%2").arg(base).arg(i);
    }
    return candidate;
}