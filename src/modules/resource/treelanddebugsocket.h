// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

// Default local-socket name of the treeland-debug remote-object host.
//
// Debug builds use a distinct name so a debug treeland can run alongside a
// release one without colliding, and a debug-built treeland-debug client
// connects to the debug instance by default (WM-342). Keep in sync with the
// TREELAND_DEBUG_SOCKET variable in misc/systemd/CMakeLists.txt, which injects
// the same value into the session unit.
inline QString treelandDebugDefaultSocketName()
{
#ifdef QT_DEBUG
    return QStringLiteral("org.deepin.dde.treeland.debug-dev");
#else
    return QStringLiteral("org.deepin.dde.treeland.debug");
#endif
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