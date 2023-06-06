// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wtexture.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"
#include "wsurfacehandler.h"

#include <qwoutput.h>

#include <QDebug>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq, WServer *server)
    : WObjectPrivate(qq)
    , server(server)
{

}

WSurfacePrivate::~WSurfacePrivate()
{

}

void WSurfacePrivate::on_commit(void *)
{
    W_Q(WSurface);

    if (Q_UNLIKELY(handle->current.width != handle->previous.width
                   || handle->current.height != handle->previous.height)) {
        Q_EMIT q->sizeChanged(QSize(handle->previous.width, handle->previous.height),
                              QSize(handle->current.width, handle->current.height));
    }

    if (Q_UNLIKELY(handle->current.buffer_width != handle->previous.buffer_width
                   || handle->current.buffer_height != handle->previous.buffer_height)) {
        Q_EMIT q->bufferSizeChanged(QSize(handle->previous.buffer_width, handle->previous.buffer_height),
                                    QSize(handle->current.buffer_width, handle->current.buffer_height));
    }

    if (Q_UNLIKELY(handle->current.scale != handle->previous.scale)) {
        Q_EMIT q->bufferScaleChanged(handle->previous.scale, handle->current.scale);
    }
}

void WSurfacePrivate::on_client_commit(void *)
{
    Q_EMIT q_func()->textureChanged();
}

void WSurfacePrivate::connect()
{
    sc.connect(&handle->events.commit, this, &WSurfacePrivate::on_commit);
    sc.connect(&handle->events.client_commit, this, &WSurfacePrivate::on_client_commit);
    sc.connect(&handle->events.destroy, &sc, &WSignalConnector::invalidate);
}

void WSurfacePrivate::updateOutputs()
{
    std::any oldOutputs = outputs;
    outputs.clear();
    wlr_surface_output *output;
    wl_list_for_each(output, &handle->current_outputs, link) {
        outputs << WOutput::fromHandle<wlr_output>(output->output);
    }
    W_Q(WSurface);
    q->notifyChanged(WSurface::ChangeType::Outputs, oldOutputs, outputs);
}

WSurface::WSurface(WServer *server, QObject *parent)
    : WSurface(*new WSurfacePrivate(this, server), parent)
{

}

WSurface::Type *WSurface::type() const
{
    return nullptr;
}

bool WSurface::testAttribute(WSurface::Attribute attr) const
{
    return false;
}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{

}

void WSurface::setHandle(WSurfaceHandle *handle)
{
    W_D(WSurface);
    d->handle = reinterpret_cast<wlr_surface*>(handle);
    d->handle->data = this;
    d->connect();
}

WSurfaceHandle *WSurface::handle() const
{
    W_DC(WSurface);
    return reinterpret_cast<WSurfaceHandle*>(d->handle);
}

WSurfaceHandle *WSurface::inputTargetAt(QPointF &globalPos) const
{
    Q_UNUSED(globalPos)
    return nullptr;
}

WSurface *WSurface::fromHandle(WSurfaceHandle *handle)
{
    auto *data = reinterpret_cast<wlr_surface*>(handle)->data;
    return reinterpret_cast<WSurface*>(data);
}

bool WSurface::inputRegionContains(const QPointF &localPos) const
{
    Q_UNUSED(localPos)
    return false;
}

WServer *WSurface::server() const
{
    W_DC(WSurface);
    return d->server;
}

WSurface *WSurface::parentSurface() const
{
    return nullptr;
}

QSize WSurface::size() const
{
    W_DC(WSurface);
    return QSize(d->handle->current.width, d->handle->current.height);
}

QSize WSurface::bufferSize() const
{
    W_DC(WSurface);
    return QSize(d->handle->current.buffer_width,
                 d->handle->current.buffer_height);
}

WLR::Transform WSurface::orientation() const
{
    W_DC(WSurface);
    return static_cast<WLR::Transform>(d->handle->current.transform);
}

int WSurface::bufferScale() const
{
    W_DC(WSurface);
    return d->handle->current.scale;
}

void WSurface::resize(const QSize &newSize)
{
    Q_UNUSED(newSize)
}

QPointF WSurface::mapToGlobal(const QPointF &localPos) const
{
    W_DC(WSurface);
    return d->handler ? d->handler->mapToGlobal(localPos) : localPos;
}

QPointF WSurface::mapFromGlobal(const QPointF &globalPos) const
{
    W_DC(WSurface);
    return d->handler ? d->handler->mapFromGlobal(globalPos) : globalPos;
}

WTextureHandle *WSurface::texture() const
{
    W_DC(WSurface);
    return reinterpret_cast<WTextureHandle*>(wlr_surface_get_texture(d->handle));
}

QPoint WSurface::textureOffset() const
{
    W_DC(WSurface);
    return QPoint(0, 0);
}

void WSurface::notifyFrameDone()
{
    W_D(WSurface);
    /* This lets the client know that we've displayed that frame and it can
    * prepare another one now if it likes. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(d->handle, &now);
}

void WSurface::enterOutput(WOutput *output)
{
    W_D(WSurface);
    Q_ASSERT(!d->outputs.contains(output));
    wlr_surface_send_enter(d->handle, output->nativeInterface<QWOutput>()->handle());

    connect(output, &WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });

    d->updateOutputs();
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    Q_ASSERT(d->outputs.contains(output));
    wlr_surface_send_leave(d->handle, output->nativeInterface<QWOutput>()->handle());

    connect(output, &WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });

    d->updateOutputs();
}

QVector<WOutput *> WSurface::outputs() const
{
    W_DC(WSurface);
    return d->outputs;
}

QPointF WSurface::position() const
{
    W_DC(WSurface);
    return d->handler ? d->handler->position() : QPointF();
}

WSurfaceHandler *WSurface::handler() const
{
    W_DC(WSurface);
    return d->handler;
}

void WSurface::setHandler(WSurfaceHandler *handler)
{
    W_D(WSurface);
    if (d->handler == handler)
        return;
    std::any old = d->handler;
    d->handler = handler;
    notifyChanged(ChangeType::Handler, old, handler);
}

void WSurface::notifyChanged(ChangeType type, std::any oldValue, std::any newValue)
{
    Q_UNUSED(type)
    Q_UNUSED(oldValue)
    Q_UNUSED(newValue)
}

void WSurface::notifyBeginState(State)
{

}

void WSurface::notifyEndState(State)
{

}

WAYLIB_SERVER_END_NAMESPACE
