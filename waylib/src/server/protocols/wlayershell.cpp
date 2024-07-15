// Copyright (C) 2023-2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayershell.h"
#include "wlayersurface.h"
#include "woutput.h"
#include "private/wglobal_p.h"

#include <qwlayershellv1.h>
#include <qwxdgshell.h>

#include <QVector>

extern "C" {
// avoid replace namespace
#include <math.h>
#define namespace scope
#define static
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWLayerShellV1;
using QW_NAMESPACE::QWLayerSurfaceV1;

class WLayerShellPrivate : public WWrapObjectPrivate
{
public:
    WLayerShellPrivate(WLayerShell *qq)
        : WWrapObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewSurface(QWLayerSurfaceV1 *layerSurface);
    void onSurfaceDestroy(QWLayerSurfaceV1 *layerSurface);
    // end slot function

    W_DECLARE_PUBLIC(WLayerShell)

    QVector<WLayerSurface*> surfaceList;
};

void WLayerShellPrivate::onNewSurface(QWLayerSurfaceV1 *layerSurface)
{
    W_Q(WLayerShell);

    auto server = q->server();
    auto surface = new WLayerSurface(layerSurface, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);

    surface->safeConnect(&QWLayerSurfaceV1::beforeDestroy, q, [this, layerSurface] {
        onSurfaceDestroy(layerSurface);
    });

    surfaceList.append(surface);
    Q_EMIT q->surfaceAdded(surface);
}

void WLayerShellPrivate::onSurfaceDestroy(QWLayerSurfaceV1 *layerSurface)
{
    auto surface = WLayerSurface::fromHandle(layerSurface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->surfaceRemoved(surface);
    surface->safeDeleteLater();
}

WLayerShell::WLayerShell(QObject *parent):
    WWrapObject(*new WLayerShellPrivate(this), nullptr)
{

}

QVector<WLayerSurface*> WLayerShell::surfaceList() const
{
    W_DC(WLayerShell);
    return d->surfaceList;
}

QByteArrayView WLayerShell::interfaceName() const
{
    return "zwlr_layer_shell_v1";
}

void WLayerShell::create(WServer *server)
{
    W_D(WLayerShell);

    auto *layer_shell = QWLayerShellV1::create(server->handle(), 4);
    connect(layer_shell, &QWLayerShellV1::newSurface, this, [d](QWLayerSurfaceV1 *surface) {
        d->onNewSurface(surface);
    });
    m_handle = layer_shell;
}

void WLayerShell::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WLayerShell);

    auto list = d->surfaceList;
    d->surfaceList.clear();

    for (auto surface : std::as_const(list)) {
        surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

wl_global *WLayerShell::global() const
{
    auto handle = nativeInterface<QWLayerShellV1>();
    return handle->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
