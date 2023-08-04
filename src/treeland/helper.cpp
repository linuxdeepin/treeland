// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WServer>
#include <WOutput>
#include <WSurfaceItem>
// TODO: Don't use private API
#include <wquickbackend_p.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwcompositor.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <QFile>
#include <QRegularExpression>

extern "C" {
#define WLR_USE_UNSTABLE
#define static
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#undef static
}

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
{

}

WSurfaceItem *Helper::resizingItem() const
{
    return m_resizingItem;
}

void Helper::setResizingItem(WSurfaceItem *newResizingItem)
{
    if (m_resizingItem == newResizingItem)
        return;
    m_resizingItem = newResizingItem;
    emit resizingItemChanged();
}

WSurfaceItem *Helper::movingItem() const
{
    return m_movingItem;
}

QString Helper::clientName(WSurface *surface) const
{
    wl_client *client = surface->handle()->handle()->resource->client;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    QString programName;
    QFile file(QString("/proc/%1/status").arg(pid));
    if (file.open(QFile::ReadOnly)) {
        programName = QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"),1,1);
    }

    qDebug() << "Program name for PID" << pid << "is" << programName;
    return programName;
}

void Helper::setMovingItem(WSurfaceItem *newMovingItem)
{
    if (m_movingItem == newMovingItem)
        return;
    m_movingItem = newMovingItem;
    emit movingItemChanged();
}

void Helper::stopMoveResize()
{
    if (surface)
        surface->setResizeing(false);

    setResizingItem(nullptr);
    setMovingItem(nullptr);

    surfaceItem = nullptr;
    surface = nullptr;
    seat = nullptr;
    resizeEdgets = {0};
}

void Helper::startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)

    surfaceItem = shell;
    this->surface = surface;
    this->seat = seat;
    resizeEdgets = {0};
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceItem);

    setMovingItem(shell);
}

void Helper::startResize(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)
    Q_ASSERT(edge != 0);

    surfaceItem = shell;
    this->surface = surface;
    this->seat = seat;
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceItem);
    surfaceSizeOfStartMoveResize = surfaceItem->size();
    resizeEdgets = edge;

    surface->setResizeing(true);
    setResizingItem(shell);
}

void Helper::cancelMoveResize(WSurfaceItem *shell)
{
    if (surfaceItem != shell)
        return;
    stopMoveResize();
}

WSurface *Helper::getFocusSurfaceFrom(QObject *object)
{
    auto item = WSurfaceItem::fromFocusObject(object);
    return item ? item->surface() : nullptr;
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    connect(output->handle(), &QWOutput::requestState, this, &Helper::onOutputRequeseState);
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    // TODO: shortcut
    if (event->type() == QEvent::KeyPress) {
        auto e = static_cast<QKeyEvent*>(event);
        emit keyEvent(e->key(), e->modifiers());
    }

    if (watched) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
            seat->setKeyboardFocusTarget(watched);
        } else if (event->type() == QEvent::MouseMove && !seat->focusWindow()) {
            // TouchMove keep focus on first window
            seat->setKeyboardFocusTarget(watched);
        }
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    if (surfaceItem && (seat == this->seat || this->seat == nullptr)) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent*>(event);

            if (resizeEdgets == 0) {
                auto increment_pos = ev->globalPosition() - cursor->lastPressedPosition();
                auto new_pos = surfacePosOfStartMoveResize + surfaceItem->parentItem()->mapFromGlobal(increment_pos);
                surfaceItem->setPosition(new_pos);
            } else {
                auto increment_pos = surfaceItem->parentItem()->mapFromGlobal(ev->globalPosition() - cursor->lastPressedPosition());
                QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfStartMoveResize);

                if (resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                if (surface->checkNewSize(geo.size().toSize())) {
                    surfaceItem->setPosition(geo.topLeft());
                    surfaceItem->setSize(geo.size());
                }
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            stopMoveResize();
        }
    }

    return false;
}

bool Helper::afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        // surfaceItem is qml type: XdgSurfaceItem
        auto xdgSurface = qvariant_cast<WToplevelSurface*>(surfaceItem->property("surface"));
        if (!xdgSurface)
            return false;
        Q_ASSERT(xdgSurface->surface() == watched);
        setActivateSurface(xdgSurface);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent*>(event)->isBeginEvent())
            setActivateSurface(nullptr);
    }

    return false;
}

WToplevelSurface *Helper::activatedSurface() const
{
    return m_activateSurface;
}

void Helper::setActivateSurface(WToplevelSurface *newActivate)
{
    if (newActivate && newActivate->doesNotAcceptFocus())
        return;

    if (m_activateSurface == newActivate)
        return;
    if (m_activateSurface)
        m_activateSurface->setActivate(false);
    m_activateSurface = newActivate;
    if (newActivate)
        newActivate->setActivate(true);
    Q_EMIT activatedSurfaceChanged();
}

void Helper::onOutputRequeseState(wlr_output_event_request_state *newState)
{
    if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
        auto output = qobject_cast<QWOutput*>(sender());

        if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            const QSize size(newState->state->custom_mode.width, newState->state->custom_mode.height);
            output->setCustomMode(size, newState->state->custom_mode.refresh);
        } else {
            output->setMode(newState->state->mode);
        }
    }
}
