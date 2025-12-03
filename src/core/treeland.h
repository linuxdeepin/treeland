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
    Q_CLASSINFO("D-Bus Interface", "org.deepin.Compositor1")
    Q_CLASSINFO("D-Bus Introspection",
                "  <interface name=\"org.deepin.Compositor1\">\n"
                "    <method name=\"ActivateWayland\">\n"
                "      <arg direction=\"in\" type=\"h\" name=\"fd\"/>\n"
                "      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
                "    </method>\n"
                "    <method name=\"XWaylandName\">\n"
                "      <arg direction=\"out\" type=\"s\" name=\"result\"/>\n"
                "      <arg direction=\"out\" type=\"ay\" name=\"auth\"/>\n"
                "    </method>\n"
                "    <signal name=\"SessionChanged\"/>\n"
                "  </interface>\n"
                "")

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

public Q_SLOTS:
    bool ActivateWayland(QDBusUnixFileDescriptor fd);
    void XWaylandName();

private:
    void quit();

    std::unique_ptr<TreelandPrivate> d_ptr;
};
} // namespace Treeland
