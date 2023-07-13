// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "qwglobal.h"
#include "wtexture.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"
#include "wsurfacehandler.h"

#include <qwoutput.h>
#include <qwcompositor.h>
#include <qwtexture.h>
#include <QDebug>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
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

wlr_surface *WSurfacePrivate::nativeHandle() const {
    Q_ASSERT(handle);
    return handle->handle();
}

void WSurfacePrivate::on_commit()
{
    W_Q(WSurface);

    if (Q_UNLIKELY(nativeHandle()->current.width != nativeHandle()->previous.width
                   || nativeHandle()->current.height != nativeHandle()->previous.height)) {
        Q_EMIT q->sizeChanged(QSize(nativeHandle()->previous.width, nativeHandle()->previous.height),
                              QSize(nativeHandle()->current.width, nativeHandle()->current.height));
    }

    if (Q_UNLIKELY(nativeHandle()->current.buffer_width != nativeHandle()->previous.buffer_width
                   || nativeHandle()->current.buffer_height != nativeHandle()->previous.buffer_height)) {
        Q_EMIT q->bufferSizeChanged(QSize(nativeHandle()->previous.buffer_width, nativeHandle()->previous.buffer_height),
                                    QSize(nativeHandle()->current.buffer_width, nativeHandle()->current.buffer_height));
    }

    if (Q_UNLIKELY(nativeHandle()->current.scale != nativeHandle()->previous.scale)) {
        Q_EMIT q->bufferScaleChanged(nativeHandle()->previous.scale, nativeHandle()->current.scale);
    }
}

void WSurfacePrivate::on_client_commit()
{
    Q_EMIT q_func()->textureChanged();
}

void WSurfacePrivate::connect()
{
    QObject::connect(handle, &QWSurface::commit, q_func(), [this] {
        on_commit();
    });
    QObject::connect(handle, &QWSurface::client_commit, q_func(), [this] {
        on_client_commit();
    });
}

void WSurfacePrivate::updateOutputs()
{
    std::any oldOutputs = outputs;
    outputs.clear();
    wlr_surface_output *output;
    wl_list_for_each(output, &nativeHandle()->current_outputs, link) {
        outputs << WOutput::fromHandle(QWOutput::from(output->output));
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

void WSurface::setHandle(QWSurface *handle)
{
    W_D(WSurface);
    d->handle = handle;
    d->handle->setData(this, this);
    d->connect();
}

QWSurface *WSurface::handle() const
{
    W_DC(WSurface);
    return d->handle;
}

QWSurface *WSurface::inputTargetAt(QPointF &globalPos) const
{
    Q_UNUSED(globalPos)
    return nullptr;
}

WSurface *WSurface::fromHandle(QWSurface *handle)
{
    return handle->getData<WSurface>();
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
    return QSize(d->nativeHandle()->current.width, d->nativeHandle()->current.height);
}

QSize WSurface::bufferSize() const
{
    W_DC(WSurface);
    return QSize(d->nativeHandle()->current.buffer_width,
                 d->nativeHandle()->current.buffer_height);
}

WLR::Transform WSurface::orientation() const
{
    W_DC(WSurface);
    return static_cast<WLR::Transform>(d->nativeHandle()->current.transform);
}

int WSurface::bufferScale() const
{
    W_DC(WSurface);
    return d->nativeHandle()->current.scale;
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

QWTexture *WSurface::texture() const
{
    W_DC(WSurface);
    auto *textureHandle = wlr_surface_get_texture(d->nativeHandle());
    if (!textureHandle)
        return nullptr;
    return QWTexture::from(textureHandle);
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
    wlr_surface_send_frame_done(d->nativeHandle(), &now);
}

void WSurface::enterOutput(WOutput *output)
{
    W_D(WSurface);
    Q_ASSERT(!d->outputs.contains(output));
    wlr_surface_send_enter(d->nativeHandle(), output->handle()->handle());

    connect(output, &WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });

    d->updateOutputs();
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    Q_ASSERT(d->outputs.contains(output));
    wlr_surface_send_leave(d->nativeHandle(), output->handle()->handle());

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

QObject *WSurface::shell() const
{
    W_DC(WSurface);
    return d->shell;
}

void WSurface::setShell(QObject *shell)
{
    W_D(WSurface);
    if (d->shell == shell)
        return;
    d->shell = shell;
    Q_EMIT shellChanged();
}

WAYLIB_SERVER_END_NAMESPACE
