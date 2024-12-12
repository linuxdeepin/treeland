// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
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
class Helper;
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

    bool testMode() const;

    bool debugMode() const;

    QmlEngine *qmlEngine() const override;
    Workspace *workspace() const override;
    RootSurfaceContainer *rootSurfaceContainer() const override;

    void blockActivateSurface(bool block) override;
    bool isBlockActivateSurface() const override;

Q_SIGNALS:
    void socketDisconnected();

public Q_SLOTS:
    bool ActivateWayland(QDBusUnixFileDescriptor fd);
    QString XWaylandName();

private:
    std::unique_ptr<TreelandPrivate> d_ptr;
};
} // namespace Treeland
