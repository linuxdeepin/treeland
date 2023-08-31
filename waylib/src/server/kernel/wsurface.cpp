// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "qwglobal.h"
#include "wtexture.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

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

WSurfacePrivate::WSurfacePrivate(WSurface *qq, QWSurface *handle)
    : WObjectPrivate(qq)
    , handle(handle)
{

}

WSurfacePrivate::~WSurfacePrivate()
{
    if (handle)
        handle->setData(this, nullptr);

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

void WSurfacePrivate::init()
{
    W_Q(WSurface);
    handle->setData(this, q);

    connect();
    updateBuffer();
    updateHasSubsurface();

    if (auto sub = QWSubsurface::tryFrom(handle))
        setSubsurface(sub);

    wlr_surface *surface = nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }
}

void WSurfacePrivate::connect()
{
    W_Q(WSurface);

    QObject::connect(handle.get(), &QWSurface::commit, q, [this] {
        on_commit();
    });
    QObject::connect(handle.get(), &QWSurface::mapped, q, &WSurface::mappedChanged);
    QObject::connect(handle.get(), &QWSurface::unmapped, q, &WSurface::mappedChanged);
    QObject::connect(handle.get(), &QWSurface::newSubsurface, q, [q, this] (QWSubsurface *sub) {
        setHasSubsurface(true);

        auto surface = ensureSubsurface(sub->handle());
        Q_EMIT q->newSubsurface(surface);

        for (auto output : outputs)
            surface->enterOutput(output);
    });
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    wlr_surface_output *output;
    wl_list_for_each(output, &nativeHandle()->current_outputs, link) {
        outputs << WOutput::fromHandle(QWOutput::from(output->output));
    }
}

void WSurfacePrivate::setPrimaryOutput(WOutput *output)
{
    W_Q(WSurface);

    primaryOutput = output;
    Q_EMIT q->primaryOutputChanged();
}

void WSurfacePrivate::setBuffer(QWBuffer *newBuffer)
{
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

    Q_EMIT q_func()->bufferChanged();
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

    auto qwsurface = QWSurface::from(subsurface->surface);
    auto surface = new WSurface(qwsurface, q_func());
    QObject::connect(qwsurface, &QWSurface::beforeDestroy, surface, &WSurface::deleteLater);

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

WSurface::WSurface(QWSurface *handle, QObject *parent)
    : WSurface(*new WSurfacePrivate(this, handle), parent)
{

}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{
    dd.init();
}

QWSurface *WSurface::handle() const
{
    W_DC(WSurface);
    return d->handle;
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
    W_DC(WSurface);
    return d->handle->pointAcceptsInput(localPos);
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

QPoint WSurface::bufferOffset() const
{
    W_DC(WSurface);
    return QPoint(d->nativeHandle()->current.dx, d->nativeHandle()->current.dy);
}

QWBuffer *WSurface::buffer() const
{
    W_DC(WSurface);
    return d->buffer;
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
    if (d->outputs.contains(output))
        return;
    wlr_surface_send_enter(d->nativeHandle(), output->handle()->handle());

    connect(output, &WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });

    d->updateOutputs();

    if (!d->primaryOutput) {
        d->primaryOutput = output;
        Q_EMIT primaryOutputChanged();
    }

    // for subsurface
    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    if (!d->outputs.contains(output))
        return;
    wlr_surface_send_leave(d->nativeHandle(), output->handle()->handle());

    connect(output, &WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });

    d->updateOutputs();

    if (d->primaryOutput == output) {
        d->primaryOutput = d->outputs.isEmpty() ? nullptr : d->outputs.last();
        Q_EMIT primaryOutputChanged();
    }

    // for subsurface
    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
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

void WSurface::map()
{
    W_D(WSurface);
    wlr_surface_map(d->nativeHandle());
}

void WSurface::unmap()
{
    W_D(WSurface);
    wlr_surface_unmap(d->nativeHandle());
}

WAYLIB_SERVER_END_NAMESPACE
