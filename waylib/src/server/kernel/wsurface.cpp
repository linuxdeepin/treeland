// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <QDebug>

extern "C" {
#include <wlr/util/edges.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq, wlr_surface *handle)
    : WWrapObjectPrivate(qq)
{
    initNativeHandle(handle, &handle->events.destroy);
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

    needsFrame = !wl_list_empty(&nativeHandle()->current.frame_callback_list);

    if (nativeHandle()->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (nativeHandle()->current.committed & WLR_SURFACE_STATE_OFFSET)
        updateBufferOffset();

    if (hasSubsurface) // Will make to true when wlr_surface new_subsurface signal
        updateHasSubsurface();

    Q_EMIT q->commit(nativeHandle()->current.committed);
}

void WSurfacePrivate::init()
{
    W_Q(WSurface);

    connect();
    updateBuffer();
    updateHasSubsurface();

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
    wlr_surface *surface = nativeHandle();

    m_commitListener.connect(&surface->events.commit, [](wl_listener *listener, void *) {
        auto *self = WScopedListener::owner<WSurfacePrivate, &WSurfacePrivate::m_commitListener>(listener);
        self->on_commit();
    });
    m_mapListener.connect(&surface->events.map, [](wl_listener *listener, void *) {
        auto *self = WScopedListener::owner<WSurfacePrivate, &WSurfacePrivate::m_mapListener>(listener);
        Q_EMIT self->q_func()->mappedChanged();
    });
    m_unmapListener.connect(&surface->events.unmap, [](wl_listener *listener, void *) {
        auto *self = WScopedListener::owner<WSurfacePrivate, &WSurfacePrivate::m_unmapListener>(listener);
        Q_EMIT self->q_func()->mappedChanged();
    });
    m_newSubsurfaceListener.connect(&surface->events.new_subsurface, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WSurfacePrivate, &WSurfacePrivate::m_newSubsurfaceListener>(listener);
        auto *sub = static_cast<wlr_subsurface*>(data);
        self->setHasSubsurface(true);

        auto surface = self->ensureSubsurface(sub);
        Q_EMIT self->q_func()->newSubsurface(surface);

        for (auto output : std::as_const(self->outputs))
            surface->enterOutput(output);
    });
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    framePacingOutput = nullptr;
    wlr_surface_output *output;
    wl_list_for_each(output, &nativeHandle()->current_outputs, link) {
        auto o = WOutput::fromHandle(output->output);
        if (!o)
            continue;
        outputs << o;

        if (!framePacingOutput
            || framePacingOutput->nativeHandle()->refresh
                < o->handle()->refresh) {
            framePacingOutput = o;
        }
    }

    updatePreferredBufferScale();
}

void WSurfacePrivate::setBuffer(wlr_buffer *newBuffer)
{
    if (buffer) {
        if (auto clientBuffer = wlr_client_buffer_get(buffer)) {
            Q_ASSERT(clientBuffer->handle()->n_ignore_locks > 0);
            clientBuffer->handle()->n_ignore_locks--;
        }
    }

    if (newBuffer) {
        if (auto clientBuffer = wlr_client_buffer_get(newBuffer)) {
            clientBuffer->handle()->n_ignore_locks++;
        }

        wlr_buffer_lock(newBuffer);
        buffer.reset(newBuffer);
    } else {
        buffer.reset(nullptr);
    }
}

void WSurfacePrivate::updateBuffer()
{
    wlr_buffer *buffer = nullptr;
    if (nativeHandle()->buffer)
        buffer = &nativeHandle()->buffer->base;

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
        wlr_fractional_scale_v1_notify_scale(nativeHandle(), maxScale);

    preferredBufferScale = qCeil(maxScale);
    preferredBufferScaleChange();
}

void WSurfacePrivate::preferredBufferScaleChange()
{
    W_Q(WSurface);
    if (handle())
        wlr_surface_set_preferred_buffer_scale(handle(), q->preferredBufferScale());
    Q_EMIT q->preferredBufferScaleChanged();
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    auto surface = new WSurface(subsurface->surface, q_func());
    return surface;
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

WSurface::WSurface(wlr_surface *handle, QObject *parent)
    : WSurface(*new WSurfacePrivate(this, handle), parent)
{

}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : WWrapObject(dd, parent)
{
    dd.init();
}

wlr_surface *WSurface::handle() const
{
    W_DC(WSurface);
    return d->handle();
}

WSurface *WSurface::fromHandle(wlr_surface *handle)
{
    return static_cast<WSurface*>(WWrapObjectPrivate::fromNativeHandle(handle));
}

bool WSurface::inputRegionContains(const QPointF &localPos) const
{
    W_DC(WSurface);
    return wlr_surface_point_accepts_input(d->handle(), localPos.x(), localPos.y());
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

wlr_buffer *WSurface::buffer() const
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
    wlr_surface_send_enter(d->nativeHandle(), output->handle());

    connect(output, &WOutput::aboutToBeInvalidated, this, [this, output] {
        leaveOutput(output);
    });
    output->safeConnect(&WOutput::scaleChanged, this, [d] {
        d->updatePreferredBufferScale();
    });

    d->updateOutputs();

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
    wlr_surface_send_leave(d->nativeHandle(), output->handle());

    output->safeDisconnect(this);
    d->updateOutputs();

    // for subsurface
    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    Q_EMIT outputLeave(output);
}

const QList<WOutput *> &WSurface::outputs() const
{
    W_DC(WSurface);
    return d->outputs;
}

WOutput *WSurface::framePacingOutput() const
{
    W_DC(WSurface);
    return d->framePacingOutput;
}

bool WSurface::isSubsurface() const
{
    return wlr_subsurface_try_from_wlr_surface(handle()) != nullptr;
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
    m_commitListener.remove();
    m_mapListener.remove();
    m_unmapListener.remove();
    m_newSubsurfaceListener.remove();
    // subsurface tracking removed: wlr_subsurface is a C struct,
    // connections are managed via WScopedListener now
    for (auto o : std::as_const(outputs))
        o->safeDisconnect(q);
}

bool WSurface::needsFrame() const
{
    W_DC(WSurface);
    return d->needsFrame;
}

bool WSurface::scheduleFrameIfNeeded()
{
    W_D(WSurface);
    if (needsFrame() && d->framePacingOutput) {
        d->needsFrame = false;
        wlr_output_schedule_frame(d->framePacingOutput->handle());
        return true;
    }
    return false;
}

WAYLIB_SERVER_END_NAMESPACE
