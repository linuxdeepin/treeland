// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wscoplistener.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

#include <wlr_all.h>

#include <QDebug>

WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq, wlr_surface *handle)
    : WWaylandResourcePrivate(qq)
{
    Q_ASSERT(handle);
    handle->data = qq;
    m_handle = handle;
}

WSurfacePrivate::~WSurfacePrivate()
{

}

wl_client *WSurfacePrivate::waylandClient() const
{
    if (auto handle = m_handle)
        return handle->resource->client;
    return nullptr;
}

void WSurfacePrivate::on_commit()
{
    W_Q(WSurface);

    needsFrame = !wl_list_empty(&m_handle->current.frame_callback_list);

    if (m_handle->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (m_handle->current.committed & WLR_SURFACE_STATE_OFFSET)
        updateBufferOffset();

    if (hasSubsurface) // Will make to true when wlr_surface::new_subsurface
        updateHasSubsurface();

    Q_EMIT q->commit(m_handle->current.committed);
}

void WSurfacePrivate::init()
{
    W_Q(WSurface);
    connect();
    updateBuffer();
    updateHasSubsurface();

    wlr_surface *surface = m_handle;
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
    q->listeners()->add(&m_handle->events.commit, this, &WSurfacePrivate::on_commit);
    q->listeners()->add(&m_handle->events.map, q, &WSurface::mappedChanged);
    q->listeners()->add(&m_handle->events.unmap, q, &WSurface::mappedChanged);
    q->listeners()->add(&m_handle->events.new_subsurface, q,
        [q, this] (wlr_subsurface *sub) {
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
    framePacingOutput = nullptr;
    wlr_surface_output *output;
    wl_list_for_each(output, &m_handle->current_outputs, link) {
        auto o = WOutput::fromHandle(output->output);
        if (!o)
            continue;
        outputs << o;

        if (!framePacingOutput
            || framePacingOutput->handle()->refresh
                < output->output->refresh) {
            framePacingOutput = o;
        }
    }

    updatePreferredBufferScale();
}

void WSurfacePrivate::setBuffer(wlr_buffer *newBuffer)
{
    if (buffer) {
        if (auto clientBuffer = wlr_client_buffer_get(buffer.get())) {
            Q_ASSERT(clientBuffer->n_ignore_locks > 0);
            clientBuffer->n_ignore_locks--;
        }
    }

    if (newBuffer) {
        if (auto clientBuffer = wlr_client_buffer_get(newBuffer)) {
            clientBuffer->n_ignore_locks++;
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
    if (m_handle->buffer)
        buffer = &m_handle->buffer->base;

    setBuffer(buffer);
}

void WSurfacePrivate::updateBufferOffset()
{
    W_Q(WSurface);
    auto dBufferOffset = QPoint(m_handle->current.dx, m_handle->current.dy);
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
        wlr_fractional_scale_v1_notify_scale(m_handle, maxScale);

    preferredBufferScale = qCeil(maxScale);
    preferredBufferScaleChange();
}

void WSurfacePrivate::preferredBufferScaleChange()
{
    W_Q(WSurface);
    if (handle())
        wlr_surface_set_preferred_buffer_scale(m_handle, q->preferredBufferScale());
    Q_EMIT q->preferredBufferScaleChanged();
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    auto surface = new WSurface(subsurface->surface);
    // The parent surface created this wrapper, so it releases it when the
    // native subsurface is destroyed (owner rule; no self-deletion in
    // WSurface). Register on the child's listeners list so the callback is
    // detached automatically when the WSurface is destroyed.
    surface->listeners(q_ptr)->add(&subsurface->surface->events.destroy, this,
        [this, surface](void *) {
            subSurfaces.removeOne(surface);
            delete surface;
        });
    subSurfaces.append(surface);

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
    setHasSubsurface(handle() && (!wl_list_empty(&m_handle->current.subsurfaces_above)
                                || !wl_list_empty(&m_handle->current.subsurfaces_below)));
}

WSurface::WSurface(wlr_surface *handle)
    : WSurface(*new WSurfacePrivate(this, handle))
{
}

WSurface::WSurface(WSurfacePrivate &dd)
    : QObject(nullptr)
    , WWaylandResource(dd, nullptr)
{
    dd.init();
}

WSurface::~WSurface()
{
    teardown();
    // Notify listeners while the object is still usable (its members are
    // alive during the destructor body).
    Q_EMIT beforeDestroy();

    W_D(WSurface);
    // Clear the reverse fromHandle() mapping. The creator destroys this
    // wrapper from the native destroy callback (or releases it while the
    // native handle is still alive), so the handle is valid here.
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    // Leave every output we entered — QObject connections to
    // WOutput::beforeDestroy are auto-disconnected by ~QObject, but we
    // must send the wl_surface leave notification and update the output
    // list while the handle is still alive.
    const auto outs = std::as_const(d->outputs);
    for (auto *o : outs)
        leaveOutput(o);

    // Subsurfaces are created by this surface, so release them here (owner
    // rule; no self-deletion in WSurface).
    QList<WSurface*> subs;
    subs.swap(d->subSurfaces);
    for (auto *sub : std::as_const(subs))
        delete sub;
}

wlr_surface *WSurface::handle() const
{
    W_DC(WSurface);
    return d->handle();
}

WSurface *WSurface::fromHandle(wlr_surface *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WSurface*>(handle->data);
}

bool WSurface::inputRegionContains(const QPointF &localPos) const
{
    W_DC(WSurface);
    return wlr_surface_point_accepts_input(d->handle(), localPos.x(), localPos.y());
}

bool WSurface::mapped() const
{
    W_DC(WSurface);
    return d->handle()->mapped;
}

QSize WSurface::size() const
{
    W_DC(WSurface);
    return QSize(d->handle()->current.width, d->handle()->current.height);
}

QSize WSurface::bufferSize() const
{
    W_DC(WSurface);
    return QSize(d->handle()->current.buffer_width,
                 d->handle()->current.buffer_height);
}

WLR::Transform WSurface::orientation() const
{
    W_DC(WSurface);
    return static_cast<WLR::Transform>(d->handle()->current.transform);
}

int WSurface::bufferScale() const
{
    W_DC(WSurface);
    return d->handle()->current.scale;
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
    wlr_surface_send_frame_done(d->handle(), &now);
}

void WSurface::enterOutput(WOutput *output)
{
    W_D(WSurface);
    if (d->outputs.contains(output))
        return;
    wlr_surface_send_enter(d->handle(), output->handle());

    QObject::connect(output, &WOutput::beforeDestroy, this, [this, output] {
        leaveOutput(output);
    });
    QObject::connect(output, &WOutput::scaleChanged, this, [d] {
        d->updatePreferredBufferScale();
    });

    d->updateOutputs();

    // for subsurface
    auto surface = d->handle();
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
    wlr_surface_send_leave(d->handle(), output->handle());

    output->disconnect(this);
    d->updateOutputs();

    // for subsurface
    auto surface = d->handle();
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

    auto surface = d->handle();
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
    wlr_surface_map(d->handle());
}

void WSurface::unmap()
{
    W_D(WSurface);
    wlr_surface_unmap(d->handle());
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
