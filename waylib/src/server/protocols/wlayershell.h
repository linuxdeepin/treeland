// Copyright (C) 2023-2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wglobal.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WLayerSurface;
class WLayerShellPrivate;
class WAYLIB_SERVER_EXPORT WLayerShell: public WWrapObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WLayerShell)

public:
    explicit WLayerShell(QObject *parent = nullptr);
    void *create();

    QVector<WLayerSurface*> surfaceList() const;
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void surfaceAdded(WLayerSurface *surface);
    void surfaceRemoved(WLayerSurface *surface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
