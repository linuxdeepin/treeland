// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "interfaces/proxyinterface.h"

#include <WServer>
#include <wsocket.h>

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <qtclasshelpermacros.h>

#include <memory>

class QmlEngine;
class QLocalSocket;
class WaylandServer;
class PluginInterface;

namespace Treeland {

class TreelandPrivate;

class Treeland
    : public QObject
    , protected QDBusContext
    , TreelandProxyInterface
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(Treeland)

public:
    explicit Treeland();
    ~Treeland();

    bool debugMode() const;

    QmlEngine *qmlEngine() const override;
    Workspace *workspace() const override;
    RootSurfaceContainer *rootSurfaceContainer() const override;

Q_SIGNALS:
    void socketDisconnected();
    void SessionChanged();
    void XWaylandAuthChanged();
    // UID-scoped: true when any seat of that user's session has an input
    // method, false when the last one is destroyed.  The session daemon uses
    // the false edge to hand a fresh Wayland socket to a surviving
    // input-method process after a replacement race.
    void InputMethodChanged(uint uid, bool connected);

public Q_SLOTS:
    bool ActivateWayland(QDBusUnixFileDescriptor fd);
    bool InputMethodState(uint uid);
#ifndef DISABLE_XWAYLAND
    void XWaylandName();
#endif

private:
    void initialize();
    void runPrestartScripts();
    void continueStartup();
    void quit();
#ifndef DISABLE_DDM
    void onLoginStateQueried(bool hasLoggedInUser);
#endif

    std::unique_ptr<TreelandPrivate> d_ptr;
};
} // namespace Treeland
