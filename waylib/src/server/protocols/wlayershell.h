// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wglobal.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

Q_MOC_INCLUDE("wlayersurface.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgShell;
class WLayerSurface;
class WLayerShellPrivate;
class WAYLIB_SERVER_EXPORT WLayerShell: public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WLayerShell)

public:
    explicit WLayerShell(WXdgShell *xdgShell);
    void *create();
    wlr_layer_shell_v1 *handle() const;

    QVector<WLayerSurface*> surfaceList() const;
    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 4;
Q_SIGNALS:
    void surfaceAdded(WLayerSurface *surface);
    void surfaceRemoved(WLayerSurface *surface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
