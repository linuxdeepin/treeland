// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "qwglobal.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

#include <qwoutput.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwtexture.h>
#include <qwbuffer.h>
#include <qwfractionalscalemanagerv1.h>
#include <QDebug>

extern "C" {
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq, qw_surface *handle)
    : WWrapObjectPrivate(qq)
{
    initHandle(handle);
}

WSurfacePrivate::~WSurfacePrivate()
{

}

wl_client *WSurfacePrivate::waylandClient() const
{
    if (auto handle = nativeHandle())
        return handle->resource->client;
    return nullptr;
}

void WSurfacePrivate::on_commit()
{
    W_Q(WSurface);

    if (nativeHandle()->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (nativeHandle()->current.committed & WLR_SURFACE_STATE_OFFSET)
        updateBufferOffset();

    if (hasSubsurface) // Will make to true when qw_surface::newSubsurface
        updateHasSubsurface();
}

void WSurfacePrivate::init()
{
    W_Q(WSurface);
    handle()->set_data(this, q);

    connect();
    updateBuffer();
    updateHasSubsurface();

    if (auto sub = qw_subsurface::try_from_wlr_surface(handle()->handle()))
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

    QObject::connect(handle(), &qw_surface::notify_commit, q, [this] {
        on_commit();
    });
    QObject::connect(handle(), &qw_surface::notify_map, q, &WSurface::mappedChanged);
    QObject::connect(handle(), &qw_surface::notify_unmap, q, &WSurface::mappedChanged);
    QObject::connect(handle(), &qw_surface::notify_new_subsurface, q, [q, this] (wlr_subsurface *sub) {
        setHasSubsurface(true);

        auto surface = ensureSubsurface(sub);
        Q_EMIT q->newSubsurface(surface);

        for (auto output : std::as_const(outputs))
            surface->enterOutput(output);
    });
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    wlr_surface_output *output;
    wl_list_for_each(output, &nativeHandle()->current_outputs, link) {
        auto qo = qw_output::from(output->output);
        if (!qo)
            continue;
        auto o = WOutput::fromHandle(qo);
        if (!o)
            continue;
        outputs << o;
    }

    updatePreferredBufferScale();
}

void WSurfacePrivate::setPrimaryOutput(WOutput *output)
{
    W_Q(WSurface);

    primaryOutput = output;
    Q_EMIT q->primaryOutputChanged();
}

void WSurfacePrivate::setBuffer(qw_buffer *newBuffer)
{
    if (buffer) {
        if (auto clientBuffer = qw_client_buffer::get(*buffer)) {
            Q_ASSERT(clientBuffer->handle()->n_ignore_locks > 0);
            clientBuffer->handle()->n_ignore_locks--;
        }
    }

    if (newBuffer) {
        if (auto clientBuffer = qw_client_buffer::get(*newBuffer)) {
            clientBuffer->handle()->n_ignore_locks++;
        }

        newBuffer->lock();
        buffer.reset(newBuffer);
    } else {
        buffer.reset(nullptr);
    }

    Q_EMIT q_func()->bufferChanged();
}

void WSurfacePrivate::updateBuffer()
{
    qw_buffer *buffer = nullptr;
    if (nativeHandle()->buffer)
        buffer = qw_buffer::from(&nativeHandle()->buffer->base);

    setBuffer(buffer);
}

void WSurfacePrivate::updateBufferOffset()
{
    W_Q(WSurface);
    auto dBufferOffset = QPoint(nativeHandle()->current.dx, nativeHandle()->current.dy);
    if (!dBufferOffset.isNull()) {
        bufferOffset += dBufferOffset;
        Q_EMIT q->bufferOffsetChanged();
    }
}

void WSurfacePrivate::updatePreferredBufferScale()
{
    if (explicitPreferredBufferScale > 0)
        return;

    float maxScale = 1.0;
    for (auto o : std::as_const(outputs))
        maxScale = std::max(o->scale(), maxScale);
    if (handle())
        qw_fractional_scale_manager_v1::notify_scale(nativeHandle(), maxScale);

    preferredBufferScale = qCeil(maxScale);
    preferredBufferScaleChange();
}

void WSurfacePrivate::preferredBufferScaleChange()
{
    W_Q(WSurface);
    if (handle())
        handle()->set_preferred_buffer_scale(q->preferredBufferScale());
    Q_EMIT q->preferredBufferScaleChanged();
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    auto qw_surface = qw_surface::from(subsurface->surface);
    auto surface = new WSurface(qw_surface, q_func());
    QObject::connect(surface->handle(), &qw_surface::before_destroy, surface, &WSurface::safeDeleteLater);

    return surface;
}

void WSurfacePrivate::setSubsurface(qw_subsurface *newSubsurface)
{
    W_Q(WSurface);
    if (subsurface == newSubsurface)
        return;
    subsurface = newSubsurface;
    QObject::connect(subsurface, &qw_subsurface::destroyed, q, &WSurface::isSubsurfaceChanged);

    if (isSubsurface != !subsurface.isNull()){
        isSubsurface = !subsurface.isNull();
        Q_EMIT q->isSubsurfaceChanged();
    }
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
    setHasSubsurface(handle() && (!wl_list_empty(&nativeHandle()->current.subsurfaces_above)
                                || !wl_list_empty(&nativeHandle()->current.subsurfaces_below)));
}

WSurface::WSurface(qw_surface *handle, QObject *parent)
    : WSurface(*new WSurfacePrivate(this, handle), parent)
{

}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : WWrapObject(dd, parent)
{
    dd.init();
}

qw_surface *WSurface::handle() const
{
    W_DC(WSurface);
    return d->handle();
}

WSurface *WSurface::fromHandle(qw_surface *handle)
{
    return handle->get_data<WSurface>();
}

WSurface *WSurface::fromHandle(wlr_surface *handle)
{
    if (auto surface = qw_surface::get(handle))
        return fromHandle(surface);
    return nullptr;
}

bool WSurface::inputRegionContains(const QPointF &localPos) const
{
    W_DC(WSurface);
    return d->handle()->point_accepts_input(localPos.x(), localPos.y());
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
    return d->bufferOffset;
}

qw_buffer *WSurface::buffer() const
{
    W_DC(WSurface);
    return d->buffer.get();
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

    output->safeConnect(&WOutput::destroyed, this, [d] {
        d->updateOutputs();
    });
    output->safeConnect(&WOutput::scaleChanged, this, [d] {
        d->updatePreferredBufferScale();
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

    Q_EMIT outputEntered(output);
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    if (!d->outputs.contains(output))
        return;
    wlr_surface_send_leave(d->nativeHandle(), output->handle()->handle());

    output->safeDisconnect(this);
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

    Q_EMIT outputLeft(output);
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
    return d->isSubsurface;
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

uint32_t WSurface::preferredBufferScale() const
{
    W_DC(WSurface);
    return d->explicitPreferredBufferScale > 0 ? d->explicitPreferredBufferScale : d->preferredBufferScale;
}

void WSurface::setPreferredBufferScale(uint32_t newPreferredBufferScale)
{
    W_D(WSurface);
    if (d->explicitPreferredBufferScale == newPreferredBufferScale)
        return;
    const auto oldScale = preferredBufferScale();
    d->explicitPreferredBufferScale = newPreferredBufferScale;
    if (d->explicitPreferredBufferScale == 0)
        d->updatePreferredBufferScale();

    if (oldScale != preferredBufferScale()) {
        d->preferredBufferScaleChange();
    }
}

void WSurface::resetPreferredBufferScale()
{
    setPreferredBufferScale(0);
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

void WSurfacePrivate::instantRelease()
{
    W_Q(WSurface);
    if (handle()) {
        handle()->set_data(nullptr, nullptr);
        handle()->disconnect(q);
        if (subsurface)
            subsurface->disconnect(q);
        for (auto o : std::as_const(outputs))
            o->safeDisconnect(q);
    }
}

WAYLIB_SERVER_END_NAMESPACE
