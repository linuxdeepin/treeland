// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicklayershell_p.h"
#include "wlayersurface.h"
#include "woutput.h"
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

    QObject::connect(layerSurface, &QWLayerSurfaceV1::beforeDestroy, q, [this] (QWLayerSurfaceV1 *data) {
        onSurfaceDestroy(data);
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
    surface->deleteLater();
}

WQuickLayerShell::WQuickLayerShell(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickLayerShellPrivate(this), nullptr)
{

}

void WQuickLayerShell::create()
{
    W_D(WQuickLayerShell);
    WQuickWaylandServerInterface::create();

    d->shell = QWLayerShellV1::create(server()->handle(), 4u);
    connect(d->shell, &QWLayerShellV1::newSurface, this, [d](QWLayerSurfaceV1 *surface) {
        d->onNewSurface(surface);
    });
}

WLayerSurfaceItem::WLayerSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WLayerSurfaceItem::~WLayerSurfaceItem()
{

}

WLayerSurface *WLayerSurfaceItem::surface() const
{
    return m_surface;
}

static inline void debugOutput(wlr_layer_surface_v1_state s)
{
    qDebug() << "committed: " << s.committed << " "
             << "configure_serial: " << s.configure_serial << "\n"
             << "anchor: " << s.anchor << " "
             << "layer: " << s.layer << " "
             << "exclusive_zone: " << s.exclusive_zone << " "
             << "keyboard_interactive: " << s.keyboard_interactive << "\n"
             << "margin: " << s.margin.left << s.margin.right << s.margin.top << s.margin.bottom << "\n"
             << "desired_width/height: " << s.desired_width << " " << s.desired_height << "\n"
             << "actual_width/height" << s.actual_width << " " << s.actual_height << "\n";
}

void WLayerSurfaceItem::setSurface(WLayerSurface *surface)
{
    if (m_surface == surface)
        return;

    m_surface = surface;
    WSurfaceItem::setSurface(surface ? surface->surface() : nullptr);

    Q_EMIT surfaceChanged();
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
    Q_ASSERT(m_surface);
    connect(m_surface->handle(), &QWLayerSurfaceV1::beforeDestroy,
            this, &WLayerSurfaceItem::releaseResources);
}

bool WLayerSurfaceItem::resizeSurface(const QSize &newSize)
{
    if (!m_surface->checkNewSize(newSize))
       return false;
    m_surface->configureSize(newSize);

    return true;
}

QRectF WLayerSurfaceItem::getContentGeometry() const
{
   return m_surface->getContentGeometry();
}

WAYLIB_SERVER_END_NAMESPACE
