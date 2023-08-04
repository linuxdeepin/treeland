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
#include <qwsubcompositor.h>
#include <qwtexture.h>
#include <qwbuffer.h>
#include <QDebug>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_subcompositor.h>
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
    if (texture)
        delete texture;

    if (buffer)
        buffer->unlock();
}

wl_client *WSurfacePrivate::waylandClient() const
{
    if (auto handle = nativeHandle())
        return handle->resource->client;
    return nullptr;
}

wlr_surface *WSurfacePrivate::nativeHandle() const {
    Q_ASSERT(handle);
    return handle->handle();
}

void WSurfacePrivate::on_commit()
{
    W_Q(WSurface);

    if (nativeHandle()->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (hasSubsurface) // Will make to true when QWSurface::newSubsurface
        updateHasSubsurface();
}

void WSurfacePrivate::connect()
{
    W_Q(WSurface);

    QObject::connect(handle, &QWSurface::commit, q, [this] {
        on_commit();
    });
    QObject::connect(handle, &QWSurface::map, q, &WSurface::mappedChanged);
    QObject::connect(handle, &QWSurface::unmap, q, &WSurface::mappedChanged);
    QObject::connect(handle, &QWSurface::newSubsurface, q, [q, this] (QWSubsurface *sub) {
        setHasSubsurface(true);
        Q_EMIT q->newSubsurface(ensureSubsurface(sub->handle()));
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

void WSurfacePrivate::setPrimaryOutput(WOutput *output)
{
    W_Q(WSurface);

    primaryOutput = output;
    Q_EMIT q->primaryOutputChanged();
}

void WSurfacePrivate::setBuffer(QWBuffer *newBuffer)
{
    if (texture)
        delete texture;

    if (buffer) {
        if (auto clientBuffer = QWClientBuffer::get(buffer)) {
            Q_ASSERT(clientBuffer->handle()->n_ignore_locks > 0);
            clientBuffer->handle()->n_ignore_locks--;
        }
        buffer->unlock();
    }

    if (newBuffer) {
        if (auto clientBuffer = QWClientBuffer::get(newBuffer)) {
            clientBuffer->handle()->n_ignore_locks++;
        }

        newBuffer->lock();
        buffer = newBuffer;
    } else {
        buffer = nullptr;
    }

    Q_EMIT q_func()->textureChanged();
}

void WSurfacePrivate::updateBuffer()
{
    QWBuffer *buffer = nullptr;
    if (handle->handle()->buffer)
        buffer = QWBuffer::from(&handle->handle()->buffer->base);

    setBuffer(buffer);
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    auto surface = new WSurface(server, q_func());
    surface->setHandle(QWSurface::from(subsurface->surface));

    return surface;
}

void WSurfacePrivate::setSubsurface(QWSubsurface *newSubsurface)
{
    W_Q(WSurface);
    if (subsurface == newSubsurface)
        return;
    subsurface = newSubsurface;
    QObject::connect(subsurface, &QWSubsurface::destroyed, q, &WSurface::isSubsurfaceChanged);

    Q_EMIT q->isSubsurfaceChanged();
}

void WSurfacePrivate::setHasSubsurface(bool newHasSubsurface)
{
    if (hasSubsurface == newHasSubsurface)
        return;
    hasSubsurface = newHasSubsurface;

    Q_EMIT q_func()->hasSubsurfaceChanged();
}

void WSurfacePrivate::updateHasSubsurface()
{
    setHasSubsurface(handle && (!wl_list_empty(&nativeHandle()->current.subsurfaces_above)
                                || !wl_list_empty(&nativeHandle()->current.subsurfaces_below)));
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
    d->updateBuffer();
    d->updateHasSubsurface();

    if (auto sub = QWSubsurface::tryFrom(handle))
        d->setSubsurface(sub);

    wlr_surface *surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        Q_EMIT newSubsurface(d->ensureSubsurface(subsurface));
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        Q_EMIT newSubsurface(d->ensureSubsurface(subsurface));
    }
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

WSurface *WSurface::fromHandle(wlr_surface *handle)
{
    if (auto surface = QWSurface::get(handle))
        return fromHandle(surface);
    return nullptr;
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

bool WSurface::mapped() const
{
    W_DC(WSurface);
    return d->nativeHandle()->mapped;
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

QRect WSurface::getContentGeometry() const
{
    W_DC(WSurface);
    return QRect(QPoint(), size());
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
    auto textureHandle = d->handle->getTexture();
    if (textureHandle)
        return textureHandle;

    if (d->texture)
        return d->texture;

    if (!d->primaryOutput)
        return nullptr;

    auto renderer = d->primaryOutput->renderer();
    if (!renderer)
        return nullptr;

    d->texture = QWTexture::fromBuffer(renderer, d->buffer);
    return d->texture;
}

QWBuffer *WSurface::buffer() const
{
    W_DC(WSurface);
    return d->buffer;
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

    if (!d->primaryOutput) {
        d->primaryOutput = output;
        Q_EMIT primaryOutputChanged();
    }
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

    if (d->primaryOutput == output) {
        d->primaryOutput = d->outputs.isEmpty() ? nullptr : d->outputs.last();
        Q_EMIT primaryOutputChanged();
    }
}

QVector<WOutput *> WSurface::outputs() const
{
    W_DC(WSurface);
    return d->outputs;
}

WOutput *WSurface::primaryOutput() const
{
    W_DC(WSurface);
    return d->primaryOutput;
}

QPointF WSurface::position() const
{
    W_DC(WSurface);
    return QPointF();
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

bool WSurface::isSubsurface() const
{
    W_DC(WSurface);
    return d->subsurface;
}

bool WSurface::hasSubsurface() const
{
    W_DC(WSurface);
    return d->hasSubsurface;
}

QList<WSurface*> WSurface::subsurfaces() const
{
    auto d = const_cast<WSurface*>(this)->d_func();
    QList<WSurface*> subsurfaeList;

    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        subsurfaeList.append(d->ensureSubsurface(subsurface));
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        subsurfaeList.append(d->ensureSubsurface(subsurface));
    }

    return subsurfaeList;
}

WAYLIB_SERVER_END_NAMESPACE
