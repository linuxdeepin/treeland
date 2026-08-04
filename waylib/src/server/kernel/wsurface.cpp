// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

#include <QDebug>
#include <QHash>

extern "C" {
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/edges.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using SurfaceRegistry = QHash<const wlr_surface *, WSurface *>;
Q_GLOBAL_STATIC(SurfaceRegistry, s_surfaces)

WSurfacePrivate::WSurfacePrivate(WSurface *qq, wlr_surface *handle)
    : WObjectPrivate(qq)
    , surfaceHandle(handle)
{
    Q_ASSERT(surfaceHandle);
    Q_ASSERT(!s_surfaces->contains(surfaceHandle));
    s_surfaces->insert(surfaceHandle, qq);
}

WSurfacePrivate::~WSurfacePrivate()
{

}

wl_client *WSurfacePrivate::waylandClient() const
{
    if (auto handle = surfaceHandle; handle && handle->resource)
        return handle->resource->client;
    return nullptr;
}

void WSurfacePrivate::on_commit()
{
    W_Q(WSurface);

    needsFrame = !wl_list_empty(&surfaceHandle->current.frame_callback_list);

    if (surfaceHandle->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (surfaceHandle->current.committed & WLR_SURFACE_STATE_OFFSET)
        updateBufferOffset();

    if (hasSubsurface)
        updateHasSubsurface();

    Q_EMIT q->commit(surfaceHandle->current.committed);
}

void WSurfacePrivate::init()
{
    W_Q(WSurface);
    connectNativeEvents();
    updateBuffer();
    updateHasSubsurface();

    wlr_surface *surface = surfaceHandle;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }
}

void WSurfacePrivate::addListener(NativeListener &listener, wl_signal *signal,
                                  NativeListener::Callback callback)
{
    listener.owner = this;
    listener.callback = callback;
    listener.listener.notify = handleNativeEvent;
    wl_list_init(&listener.listener.link);
    wl_signal_add(signal, &listener.listener);
}

void WSurfacePrivate::handleNativeEvent(wl_listener *listener, void *data)
{
    NativeListener *native;
    native = wl_container_of(listener, native, listener);
    native->callback(native->owner, data);
}

void WSurfacePrivate::connectNativeEvents()
{
    addListener(commitListener, &surfaceHandle->events.commit, [](WSurfacePrivate *self, void *) {
        self->on_commit();
    });
    addListener(mapListener, &surfaceHandle->events.map, [](WSurfacePrivate *self, void *) {
        Q_EMIT self->q_func()->mappedChanged();
    });
    addListener(unmapListener, &surfaceHandle->events.unmap, [](WSurfacePrivate *self, void *) {
        Q_EMIT self->q_func()->mappedChanged();
    });
    addListener(newSubsurfaceListener, &surfaceHandle->events.new_subsurface,
                [](WSurfacePrivate *self, void *data) {
        self->setHasSubsurface(true);
        auto *surface = self->ensureSubsurface(static_cast<wlr_subsurface *>(data));
        Q_EMIT self->q_func()->newSubsurface(surface);
        for (auto *output : std::as_const(self->outputs))
            surface->enterOutput(output);
    });
    addListener(destroyListener, &surfaceHandle->events.destroy, [](WSurfacePrivate *self, void *) {
        auto *surface = self->surfaceHandle;
        self->disconnectNativeEvents();
        s_surfaces->remove(surface);
        self->surfaceHandle = nullptr;
        self->q_func()->safeDeleteLater();
    });
}

void WSurfacePrivate::disconnectNativeEvents()
{
    for (NativeListener *listener : {
             &commitListener,
             &mapListener,
             &unmapListener,
             &newSubsurfaceListener,
             &destroyListener,
         }) {
        if (!wl_list_empty(&listener->listener.link)) {
            wl_list_remove(&listener->listener.link);
            wl_list_init(&listener->listener.link);
        }
    }
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    framePacingOutput = nullptr;
    wlr_surface_output *output;
    wl_list_for_each(output, &surfaceHandle->current_outputs, link) {
        auto o = WOutput::fromHandle(output->output);
        if (!o)
            continue;
        outputs << o;

        if (!framePacingOutput
            || framePacingOutput->handle()->refresh < output->output->refresh) {
            framePacingOutput = o;
        }
    }

    updatePreferredBufferScale();
}

void WSurfacePrivate::BufferUnlocker::operator()(wlr_buffer *buffer) const
{
    wlr_buffer_unlock(buffer);
}

void WSurfacePrivate::setBuffer(wlr_buffer *newBuffer)
{
    if (buffer) {
        if (auto *clientBuffer = wlr_client_buffer_get(buffer.get())) {
            Q_ASSERT(clientBuffer->n_ignore_locks > 0);
            clientBuffer->n_ignore_locks--;
        }
    }

    if (newBuffer) {
        if (auto *clientBuffer = wlr_client_buffer_get(newBuffer)) {
            clientBuffer->n_ignore_locks++;
        }

        buffer.reset(wlr_buffer_lock(newBuffer));
    } else {
        buffer.reset(nullptr);
    }
}

void WSurfacePrivate::updateBuffer()
{
    wlr_buffer *buffer = nullptr;
    if (surfaceHandle->buffer)
        buffer = &surfaceHandle->buffer->base;

    setBuffer(buffer);
}

void WSurfacePrivate::updateBufferOffset()
{
    W_Q(WSurface);
    auto dBufferOffset = QPoint(surfaceHandle->current.dx, surfaceHandle->current.dy);
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
    if (surfaceHandle)
        wlr_fractional_scale_v1_notify_scale(surfaceHandle, maxScale);

    preferredBufferScale = qCeil(maxScale);
    preferredBufferScaleChange();
}

void WSurfacePrivate::preferredBufferScaleChange()
{
    W_Q(WSurface);
    if (surfaceHandle)
        wlr_surface_set_preferred_buffer_scale(surfaceHandle, q->preferredBufferScale());
    Q_EMIT q->preferredBufferScaleChanged();
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    return new WSurface(subsurface->surface, q_func());
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
    setHasSubsurface(surfaceHandle && (!wl_list_empty(&surfaceHandle->current.subsurfaces_above)
                                      || !wl_list_empty(&surfaceHandle->current.subsurfaces_below)));
}

WSurface::WSurface(wlr_surface *handle, QObject *parent)
    : WSurface(*new WSurfacePrivate(this, handle), parent)
{

}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
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
    return s_surfaces->value(handle);
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

    connect(output, &WOutput::aboutToBeInvalidated, this, [this, output] {
        leaveOutput(output);
    });
    output->safeConnect(&WOutput::scaleChanged, this, [d] {
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

    output->safeDisconnect(this);
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

void WSurfacePrivate::instantRelease()
{
    W_Q(WSurface);
    if (surfaceHandle) {
        disconnectNativeEvents();
        s_surfaces->remove(surfaceHandle);
        surfaceHandle = nullptr;
    }
    for (auto *output : std::as_const(outputs))
        output->safeDisconnect(q);
    setBuffer(nullptr);
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
