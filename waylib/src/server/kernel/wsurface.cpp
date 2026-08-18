// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wscoplistener.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"
#include "wsubsurface.h"
#include "wayliblogging.h"

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

    updateStandardSubsurfaces();

    Q_EMIT q->commit(m_handle->current.committed);
}

void WSurfacePrivate::init()
{
    W_Q(WSurface);
    connect();
    updateBuffer();
    updateStandardSubsurfaces();
}

void WSurfacePrivate::connect()
{
    W_Q(WSurface);
    q->listeners()->add(&m_handle->events.commit, this, &WSurfacePrivate::on_commit);
    q->listeners()->add(&m_handle->events.map, q, &WSurface::mappedChanged);
    q->listeners()->add(&m_handle->events.unmap, q, &WSurface::mappedChanged);
    q->listeners()->add(&m_handle->events.new_subsurface, q,
        [q, this] (wlr_subsurface *sub) {
        auto *subsurface = ensureSubsurface(sub);
        updateStandardSubsurfaces();

        for (auto *output : std::as_const(outputs))
            subsurface->surface()->enterOutput(output);
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

WSubsurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurfaceHandle)
{
    W_Q(WSurface);

    for (auto *subsurface : std::as_const(subsurfaces)) {
        if (subsurface->surface()->handle() == subsurfaceHandle->surface) {
            return subsurface;
        }
    }

    auto *childSurface = new WSurface(subsurfaceHandle->surface);
    auto *subsurface = new WSubsurface(WSubsurface::Type::Standard, q, childSurface);
    WSubsurface::Place place = WSubsurface::Place::Above;
    wlr_subsurface *current;
    wl_list_for_each(current, &m_handle->current.subsurfaces_below, current.link) {
        if (current == subsurfaceHandle) {
            place = WSubsurface::Place::Below;
            break;
        }
    }
    subsurface->setPlace(place);
    subsurface->setPosition(QPointF(subsurfaceHandle->current.x, subsurfaceHandle->current.y));
    subsurfaces.append(subsurface);
    childSurface->d_func()->subsurface = subsurface;

    // Own this destroy listener on the child wrapper so deleting that wrapper
    // during the callback also detaches the listener before wlroots asserts
    // the signal list is empty at the end of native teardown.
    childSurface->listeners()->add(&subsurfaceHandle->events.destroy,
        [this, subsurface](void *) {
            releaseSubsurface(subsurface);
        });
    QObject::connect(childSurface, &WSurface::mappedChanged, subsurface, [subsurface] {
        if (auto *child = subsurface->surface())
            subsurface->setMapped(child->mapped());
    });

    setHasSubsurface(true);
    Q_EMIT q->subsurfaceAdded(subsurface);
    return subsurface;
}

WSubsurface *WSurfacePrivate::addRemoteSubsurface(wlr_surface *childHandle)
{
    W_Q(WSurface);
    Q_ASSERT(childHandle);
    auto *childSurface = new WSurface(childHandle);

    auto *subsurface = new WSubsurface(WSubsurface::Type::Remote, q, childSurface);
    remoteAbove.append(subsurface);
    childSurface->d_func()->subsurface = subsurface;
    // A remote subsurface has no native wlr_subsurface destroy event. Its
    // wl_surface may be destroyed directly when the child client disconnects,
    // so release the wrapper before wlroots asserts that surface listeners are
    // gone.
    childSurface->listeners()->add(&childHandle->events.destroy,
        [this, subsurface](void *) {
            releaseSubsurface(subsurface);
        });
    rebuildTotalSubsurfaces();
    Q_EMIT q->subsurfaceAdded(subsurface);
    return subsurface;
}

void WSurfacePrivate::removeSubsurface(WSubsurface *subsurface)
{
    W_Q(WSurface);
    if (!subsurface)
        return;

    auto &targetList = (subsurface->type() == WSubsurface::Type::Remote)
        ? (subsurface->place() == WSubsurface::Place::Below ? remoteBelow : remoteAbove)
        : (subsurface->place() == WSubsurface::Place::Below ? standardBelow : standardAbove);

    if (!targetList.removeOne(subsurface)) {
        qCCritical(lcWlSurface) << "Failed to remove subsurface from target list:" << subsurface;
        return;
    }

    if (auto *child = subsurface->surface()) {
        child->d_func()->subsurface = nullptr;
    }
    rebuildTotalSubsurfaces();
    Q_EMIT q->subsurfaceRemoved(subsurface);
}

void WSurfacePrivate::releaseSubsurface(WSubsurface *subsurface)
{
    if (!subsurface)
        return;

    auto *childSurface = subsurface->surface();

    removeSubsurface(subsurface);
    delete subsurface;

    delete childSurface;
}

void WSurfacePrivate::updateStandardSubsurfaces()
{
    if (subsurfaces.isEmpty()
        && wl_list_empty(&m_handle->current.subsurfaces_below)
        && wl_list_empty(&m_handle->current.subsurfaces_above)) {
        return;
    }

    setSubsurfaceOrder(this->remoteBelow, this->remoteAbove);
}

void WSurfacePrivate::setSubsurfaceOrder(const QList<WSubsurface *> &newRemoteBelow,
                                         const QList<WSubsurface *> &newRemoteAbove)
{
    W_Q(WSurface);

    // 1. Process Remote subsurfaces
    auto filterRemote = [q](const QList<WSubsurface *> &list, WSubsurface::Place place) {
        QList<WSubsurface *> result;
        for (auto *sub : list) {
            if (!sub || sub->parentSurface() != q || sub->type() != WSubsurface::Type::Remote)
                continue;
            sub->setPlace(place);
            result.append(sub);
        }
        return result;
    };

    this->remoteBelow = filterRemote(newRemoteBelow, WSubsurface::Place::Below);
    this->remoteAbove = filterRemote(newRemoteAbove, WSubsurface::Place::Above);

    // 2. Snapshot and process native Standard subsurfaces
    QList<QPair<WSubsurface *, QPointF>> positionUpdates;
    auto processStandard = [this, &positionUpdates](wl_list *head, WSubsurface::Place place, QList<WSubsurface *> &outList) {
        outList.clear();
        QList<wlr_subsurface *> nativeList;
        wlr_subsurface *nativeSubsurface;
        wl_list_for_each(nativeSubsurface, head, current.link) {
            nativeList.append(nativeSubsurface);
        }
        for (auto *native : std::as_const(nativeList)) {
            auto *subsurface = ensureSubsurface(native);
            subsurface->setPlace(place);
            positionUpdates.append({subsurface, QPointF(native->current.x, native->current.y)});
            outList.append(subsurface);
        }
    };

    processStandard(&m_handle->current.subsurfaces_below, WSubsurface::Place::Below, this->standardBelow);
    processStandard(&m_handle->current.subsurfaces_above, WSubsurface::Place::Above, this->standardAbove);

    rebuildTotalSubsurfaces();

    for (const auto &[subsurface, position] : std::as_const(positionUpdates))
        subsurface->setPosition(position);
}

void WSurfacePrivate::rebuildTotalSubsurfaces()
{
    const QList<WSubsurface *> ordered = standardBelow + remoteBelow + standardAbove + remoteAbove;
    const bool orderChanged = (subsurfaces != ordered);
    if (orderChanged)
        subsurfaces = ordered;

    setHasSubsurface(!subsurfaces.isEmpty());
    if (orderChanged)
        Q_EMIT q_func()->subsurfaceOrderChanged();
}

void WSurfacePrivate::setHasSubsurface(bool newHasSubsurface)
{
    if (hasSubsurface == newHasSubsurface)
        return;
    hasSubsurface = newHasSubsurface;

    Q_EMIT q_func()->hasSubsurfaceChanged();
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

    // Release all remaining subsurfaces. Their dedicated child wrappers are
    // reclaimed in releaseSubsurface().
    const auto subs = d->subsurfaces;
    for (auto *sub : subs)
        d->releaseSubsurface(sub);
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
        d->ensureSubsurface(subsurface)->surface()->enterOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->surface()->enterOutput(output);
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
        d->ensureSubsurface(subsurface)->surface()->leaveOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->surface()->leaveOutput(output);
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
    W_DC(WSurface);
    return d->subsurface != nullptr;
}

bool WSurface::hasSubsurface() const
{
    W_DC(WSurface);
    return d->hasSubsurface;
}

const QList<WSubsurface *> &WSurface::subsurfaces() const
{
    W_DC(WSurface);
    return d->subsurfaces;
}

QList<WSubsurface *> WSurface::subsurfacesBelow() const
{
    QList<WSubsurface *> result;
    const auto &subsurfaces = this->subsurfaces();
    for (auto *subsurface : subsurfaces) {
        if (subsurface->place() == WSubsurface::Place::Below)
            result.append(subsurface);
    }
    return result;
}

QList<WSubsurface *> WSurface::subsurfacesAbove() const
{
    QList<WSubsurface *> result;
    const auto &subsurfaces = this->subsurfaces();
    for (auto *subsurface : subsurfaces) {
        if (subsurface->place() == WSubsurface::Place::Above)
            result.append(subsurface);
    }
    return result;
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

WSubsurface *WSurface::addRemoteSubsurface(wlr_surface *childHandle)
{
    W_D(WSurface);
    return d->addRemoteSubsurface(childHandle);
}

void WSurface::removeSubsurface(WSubsurface *subsurface)
{
    W_D(WSurface);
    d->releaseSubsurface(subsurface);
}

void WSurface::setRemoteSubsurfaceOrder(const QList<WSubsurface *> &below,
                                        const QList<WSubsurface *> &above)
{
    W_D(WSurface);
    d->setSubsurfaceOrder(below, above);
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

#include "moc_wsurface.cpp"
