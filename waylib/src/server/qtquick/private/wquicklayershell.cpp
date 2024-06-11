// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicklayershell_p.h"
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

class WQuickLayerShellPrivate : public WObjectPrivate
{
public:
    WQuickLayerShellPrivate(WQuickLayerShell *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewSurface(QWLayerSurfaceV1 *layerSurface);
    void onSurfaceDestroy(QWLayerSurfaceV1 *layerSurface);
    // end slot function

    W_DECLARE_PUBLIC(WQuickLayerShell)

    QWLayerShellV1 *shell = nullptr;
    QVector<WLayerSurface*> surfaceList;
};

void WQuickLayerShellPrivate::onNewSurface(QWLayerSurfaceV1 *layerSurface)
{
    W_Q(WQuickLayerShell);

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

void WQuickLayerShellPrivate::onSurfaceDestroy(QWLayerSurfaceV1 *layerSurface)
{
    auto surface = WLayerSurface::fromHandle(layerSurface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->surfaceRemoved(surface);
    surface->safeDeleteLater();
}

WQuickLayerShell::WQuickLayerShell(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickLayerShellPrivate(this), nullptr)
{

}

WServerInterface *WQuickLayerShell::create()
{
    W_D(WQuickLayerShell);

    d->shell = QWLayerShellV1::create(server()->handle(), 4u);
    connect(d->shell, &QWLayerShellV1::newSurface, this, [d](QWLayerSurfaceV1 *surface) {
        d->onNewSurface(surface);
    });

    return new WServerInterface(d->shell, d->shell->handle()->global);
}

WLayerSurfaceItem::WLayerSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WLayerSurfaceItem::~WLayerSurfaceItem()
{

}

inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WLayerSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
}

void WLayerSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(layerSurface());
    connect(layerSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WLayerSurfaceItem::releaseResources);
}

QRectF WLayerSurfaceItem::getContentGeometry() const
{
   return layerSurface()->getContentGeometry();
}

WAYLIB_SERVER_END_NAMESPACE
