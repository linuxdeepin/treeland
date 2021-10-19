/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wsurface.h"
#include "wtexture.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"
#include "wsurfacelayout.h"

#include <QDebug>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq)
    : WObjectPrivate(qq)
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
        Q_EMIT q->sizeChanged();
        Q_EMIT q->effectiveSizeChanged();
    }

    Q_EMIT q->textureChanged();
}

void WSurfacePrivate::connect()
{
    sc.connect(&handle->events.commit, this, &WSurfacePrivate::on_commit);
    sc.connect(&handle->events.destroy, &sc, &WSignalConnector::invalidate);
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    wlr_surface_output *output;
    wl_list_for_each(output, &handle->current_outputs, link) {
        outputs << WOutput::fromHandle<wlr_output>(output->output);
    }
    W_Q(WSurface);
    q->notifyChanged(WSurface::ChangeType::Outputs);
}

WSurface::WSurface(QObject *parent)
    : WSurface(*new WSurfacePrivate(this), parent)
{

}

WSurface::Type *WSurface::type() const
{
    return nullptr;
}

WSurface::Attributes WSurface::attributes() const
{
    return Attributes();
}

WSurface::WSurface(WSurfacePrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{

}

WSurfaceHandle *WSurface::handle() const
{
    W_DC(WSurface);
    return reinterpret_cast<WSurfaceHandle*>(d->handle);
}

WSurfaceHandle *WSurface::inputTargetAt(qreal scale, QPointF &globalPos) const
{
    Q_UNUSED(scale)
    Q_UNUSED(globalPos)
    return nullptr;
}

WSurface *WSurface::fromHandle(WSurfaceHandle *handle)
{
    auto *data = reinterpret_cast<wlr_surface*>(handle)->data;
    return reinterpret_cast<WSurface*>(data);
}

bool WSurface::inputRegionContains(qreal scale, const QPointF &globalPos) const
{
    Q_UNUSED(scale)
    Q_UNUSED(globalPos)
    return false;
}

WServer *WSurface::server() const
{
    return WServer::fromThread(thread());
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

QSizeF WSurface::effectiveSize() const
{
    return toEffectiveSize(attachedOutput()->scale(), size());
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

int WSurface::scale() const
{
    W_DC(WSurface);
    return d->handle->current.scale;
}

void WSurface::resize(const QSize &newSize)
{
    Q_UNUSED(newSize)
}

QPointF WSurface::positionToGlobal(const QPointF &localPos) const
{
    QPointF pos = localPos;
    auto *parent = parentSurface();

    while (parent) {
        pos += parent->position();
        parent = parent->parentSurface();
    }

    return pos;
}

QPointF WSurface::positionFromGlobal(const QPointF &globalPos) const
{
    return globalPos - positionToGlobal(position());
}

WTextureHandle *WSurface::texture() const
{
    W_DC(WSurface);
    return reinterpret_cast<WTextureHandle*>(wlr_surface_get_texture(d->handle));
}

QPoint WSurface::textureOffset() const
{
    W_DC(WSurface);
    return QPoint(d->handle->sx, d->handle->sy);
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
    wlr_surface_send_enter(d->handle, output->nativeInterface<wlr_output>());

    connect(output, &WOutput::destroyed, this, [d]{
        d->updateOutputs();
    });

    d->updateOutputs();
    notifyChanged(ChangeType::Outputs);
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    Q_ASSERT(d->outputs.contains(output));
    wlr_surface_send_leave(d->handle, output->nativeInterface<wlr_output>());

    connect(output, &WOutput::destroyed, this, [d]{
        d->updateOutputs();
    });

    d->updateOutputs();
    notifyChanged(ChangeType::Outputs);
}

QVector<WOutput *> WSurface::currentOutputs() const
{
    W_DC(WSurface);
    return d->outputs;
}

WOutput *WSurface::attachedOutput() const
{
    W_DC(WSurface);
    const auto parent = parentSurface();
    return parent ? parent->attachedOutput() : d->attachedOutput.data();
}

QPointF WSurface::position() const
{
    W_DC(WSurface);
    return d->layout ? d->layout->position(this) : QPointF();
}

QPointF WSurface::effectivePosition() const
{
    return toEffectivePos(attachedOutput()->scale(), position());
}

WSurfaceLayout *WSurface::layout() const
{
    W_DC(WSurface);
    return d->layout;
}

void WSurface::setLayout(WSurfaceLayout *layout)
{
    W_D(WSurface);
    if (d->layout == layout)
        return;
    d->layout = layout;
    notifyChanged(ChangeType::Layout);
}

void WSurface::notifyChanged(ChangeType type)
{
    if (type == ChangeType::Position) {
        Q_EMIT positionChanged();
        Q_EMIT effectivePositionChanged();
    } else if (type == ChangeType::AttachedOutput) {
        W_D(WSurface);
        if (d->attachedOutput) {
            disconnect(d->attachedOutput.data(), &WOutput::scaleChanged,
                       this, &WSurface::effectivePositionChanged);
            disconnect(d->attachedOutput.data(), &WOutput::scaleChanged,
                       this, &WSurface::effectiveSizeChanged);
        }

        d->attachedOutput = d->layout->output(this);

        connect(d->attachedOutput.data(), &WOutput::scaleChanged,
                this, &WSurface::effectivePositionChanged);
        connect(d->attachedOutput.data(), &WOutput::scaleChanged,
                this, &WSurface::effectiveSizeChanged);

        Q_EMIT effectivePositionChanged();
        Q_EMIT effectiveSizeChanged();
    }
}

void WSurface::notifyBeginState(State)
{

}

void WSurface::notifyEndState(State)
{

}

void WSurface::setHandle(WSurfaceHandle *handle)
{
    W_D(WSurface);
    d->handle = reinterpret_cast<wlr_surface*>(handle);
    d->handle->data = this;
    d->connect();
}

WAYLIB_SERVER_END_NAMESPACE
