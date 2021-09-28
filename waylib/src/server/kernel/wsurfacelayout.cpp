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
#include "wsurfacelayout.h"
#include "private/wsurfacelayout_p.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wseat.h"
#include "wcursor.h"

#include <QMouseEvent>

WAYLIB_SERVER_BEGIN_NAMESPACE

class EventGrabber : public QObject {
public:
    EventGrabber(WSeat *seat, QObject *parent)
        : QObject(parent)
        , m_seat(seat) {
        seat2GrabberMap[seat] = this;
    }
    ~EventGrabber() {
        seat2GrabberMap.remove(m_seat);
    }

    void startMove(WSurface *surface, int serial);
    void startResize(WSurface *surface, Qt::Edges edge, int serial);

    inline void stop() {
        m_seat->setEventGrabber(nullptr);
        surface->notifyEndState(type);
        delete this;
    }

    bool event(QEvent *event) override;

    WSurface::State type;
    WSeat *m_seat;
    WSurface *surface;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfstartMoveResize;
    Qt::Edges resizeEdgets;
    static QHash<WSeat*, EventGrabber*> seat2GrabberMap;
};
QHash<WSeat*, EventGrabber*> EventGrabber::seat2GrabberMap;

void WSurfaceLayoutPrivate::updateSurfaceOutput(WSurface *surface)
{
    if (surface->parentSurface()) {
        return;
    }

    W_Q(WSurfaceLayout);
    WOutput *new_output = nullptr;
    const QRectF surface_geometry(q->position(surface), surface->size());

    Q_FOREACH(WOutput *output, outputLayout->outputList()) {
        const QRectF output_geometry(output->position(), output->transformedSize());
        if (output_geometry.contains(surface_geometry.center())) {
            new_output = output;
            break;
        }
    }

    // Whenever, the window must attch to a screen, so shouldn't update the output of
    // the surface when not found a new screen object.
    if (new_output) {
        q->setOutput(surface, new_output);
    }
}

EventGrabber *WSurfaceLayoutPrivate::createEventGrabber(WSurface *surface, WSeat *seat)
{
    if (seat->eventGrabber())
        return nullptr;

    if (EventGrabber::seat2GrabberMap.contains(seat))
        return nullptr;
    W_Q(WSurfaceLayout);
    Q_ASSERT(q->isMapped(surface));
    return new EventGrabber(seat, q);
}

WSurfaceLayout::WSurfaceLayout(WOutputLayout *outputLayout, QObject *parent)
    : WSurfaceLayout(*new WSurfaceLayoutPrivate(this), outputLayout, parent)
{

}

WSurfaceLayout::WSurfaceLayout(WSurfaceLayoutPrivate &dd, WOutputLayout *outputLayout, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{
    d_func()->outputLayout = outputLayout;
}

WOutputLayout *WSurfaceLayout::outputLayout() const
{
    W_DC(WSurfaceLayout);
    return d->outputLayout;
}

QVector<WSurface *> WSurfaceLayout::surfaceList() const
{
    W_DC(WSurfaceLayout);
    return d->surfaceList;
}

void WSurfaceLayout::add(WSurface *surface)
{
    W_D(WSurfaceLayout);

    Q_ASSERT(!d->surfaceList.contains(surface));
    Q_ASSERT(!surface->layout());
    d->surfaceList << surface;
    surface->setLayout(this);
    d->updateSurfaceOutput(surface);

    Q_EMIT surfaceAdded(surface);
}

void WSurfaceLayout::remove(WSurface *surface)
{
    W_D(WSurfaceLayout);

    Q_ASSERT(d->surfaceList.contains(surface));
    Q_ASSERT(surface->layout() == this);
    surface->setLayout(nullptr);
    d->surfaceList.removeOne(surface);

    Q_EMIT surfaceRemoved(surface);
}

void WSurfaceLayout::requestMove(WSurface *surface, WSeat *seat, quint32 serial)
{
    W_D(WSurfaceLayout);

    if (auto grabber = d->createEventGrabber(surface, seat))
        grabber->startMove(surface, serial);
}

void WSurfaceLayout::requestResize(WSurface *surface, WSeat *seat, Qt::Edges edge, quint32 serial)
{
    W_D(WSurfaceLayout);

    if (auto grabber = d->createEventGrabber(surface, seat))
        grabber->startResize(surface, edge, serial);
}

void WSurfaceLayout::requestMaximize(WSurface *surface)
{
    Q_UNUSED(surface)
}

void WSurfaceLayout::requestUnmaximize(WSurface *surface)
{
    Q_UNUSED(surface)
}

QPointF WSurfaceLayout::position(WSurface *surface) const
{
    W_DC(WSurfaceLayout);
    const auto &data = d->surfaceDataMap.value(surface);
    return data.pos;
}

bool WSurfaceLayout::setPosition(WSurface *surface, const QPointF &pos)
{
    W_D(WSurfaceLayout);
    auto &data = d->surfaceDataMap[surface];
    if (data.pos == pos)
        return false;
    data.pos = pos;
    surface->notifyChanged(WSurface::ChangeType::Position);
    d->updateSurfaceOutput(surface);
    return true;
}

WOutput *WSurfaceLayout::output(WSurface *surface) const
{
    W_DC(WSurfaceLayout);
    const auto &data = d->surfaceDataMap.value(surface);
    return data.output;
}

bool WSurfaceLayout::setOutput(WSurface *surface, WOutput *output)
{
    W_D(WSurfaceLayout);
    auto &data = d->surfaceDataMap[surface];
    if (data.output == output)
        return false;
    data.output = output;
    surface->notifyChanged(WSurface::ChangeType::AttachedOutput);
    return true;
}

void EventGrabber::startMove(WSurface *surface, int serial)
{
    Q_UNUSED(serial)
    if (!m_seat->attachedCursor()
            || m_seat->attachedCursor()->state() != Qt::LeftButton) {
        stop();
        return;
    }

    type = WSurface::State::Move;
    this->surface = surface;
    surfacePosOfStartMoveResize = surface->position();

    surface->notifyBeginState(type);
    m_seat->setEventGrabber(this);
}

void EventGrabber::startResize(WSurface *surface, Qt::Edges edge, int serial)
{
    Q_UNUSED(serial)
    if (!m_seat->attachedCursor()
            || m_seat->attachedCursor()->state() != Qt::LeftButton) {
        stop();
        return;
    }

    type = WSurface::State::Resize;
    this->surface = surface;
    surfacePosOfStartMoveResize = surface->position();
    surfaceSizeOfstartMoveResize = surface->size();
    resizeEdgets = edge;

    surface->notifyBeginState(type);
    m_seat->setEventGrabber(this);
}

bool EventGrabber::event(QEvent *event)
{
    if (Q_LIKELY(event->type() == QEvent::MouseMove)) {
        auto cursor = m_seat->attachedCursor();
        Q_ASSERT(cursor);
        QMouseEvent *ev = static_cast<QMouseEvent*>(event);

        if (type == WSurface::State::Move) {
            auto increment_pos = ev->localPos() - cursor->lastPressedPosition();
            increment_pos = surface->fromEffectivePos(surface->attachedOutput()->scale(), increment_pos);
            auto new_pos = surfacePosOfStartMoveResize + increment_pos;
            surface->layout()->setPosition(surface, new_pos);
        } else if (type == WSurface::State::Resize) {
            auto increment_pos = ev->localPos() - cursor->lastPressedPosition();
            increment_pos = surface->fromEffectivePos(surface->attachedOutput()->scale(), increment_pos);
            QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfstartMoveResize);

            if (resizeEdgets & Qt::LeftEdge)
                geo.setLeft(geo.left() + increment_pos.x());
            if (resizeEdgets & Qt::TopEdge)
                geo.setTop(geo.top() + increment_pos.y());

            if (resizeEdgets & Qt::RightEdge)
                geo.setRight(geo.right() + increment_pos.x());
            if (resizeEdgets & Qt::BottomEdge)
                geo.setBottom(geo.bottom() + increment_pos.y());

            surface->layout()->setPosition(surface, geo.topLeft());
            surface->resize(geo.size().toSize());
        } else {
            Q_ASSERT_X(false, __FUNCTION__, "Not support surface state request");
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        stop();
    } else {
        event->ignore();
        return false;
    }

    return true;
}

WAYLIB_SERVER_END_NAMESPACE
