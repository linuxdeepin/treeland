// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "seatsurfacemanager.h"
#include "rootsurfacecontainer.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "common/treelandlogging.h"

#include <winputdevice.h>
#include <woutput.h>
#include <woutputlayout.h>
#include <woutputitem.h>
#include <wxdgtoplevelsurface.h>
#include <wseat.h>
#include <qwoutput.h>

#include <QDateTime>

WAYLIB_SERVER_USE_NAMESPACE

SeatSurfaceManager::SeatSurfaceManager(WSeat *seat, RootSurfaceContainer *parent)
    : QObject(parent)
    , m_seat(seat)
    , m_rootContainer(parent)
{
    Q_ASSERT(seat);
    Q_ASSERT(parent);
}

SeatSurfaceManager::~SeatSurfaceManager()
{
    endMoveResize();
}

void SeatSurfaceManager::setActivatedSurface(SurfaceWrapper *surface, Qt::FocusReason reason)
{
    Q_UNUSED(reason);
    if (m_activatedSurface == surface)
        return;

    m_activatedSurface = surface;
    m_keyboardFocusSurface = surface;
    Q_EMIT activatedSurfaceChanged(surface);
}

void SeatSurfaceManager::setKeyboardFocusSurface(SurfaceWrapper *surface)
{
    if (m_keyboardFocusSurface == surface)
        return;

    auto *oldSurface = m_keyboardFocusSurface;
    // Clear focus from old surface if no other seat has it
    if (oldSurface) {
        bool otherSeatHasFocus = false;
        do {
            auto *helper = Helper::instance();
            if (!helper)
                break;

            auto *seatManager = helper->seatManager();
            if (!seatManager)
                break;

            for (auto *otherSeat : seatManager->seats()) {
                if (otherSeat == m_seat)
                    continue;
                auto *otherContainer = helper->rootSurfaceContainer()->getSeatContainer(otherSeat);
                if (otherContainer && otherContainer->keyboardFocusSurface() == oldSurface) {
                    otherSeatHasFocus = true;
                    break;
                }
            }
        } while (false);

        if (!otherSeatHasFocus)
            oldSurface->setFocus(false, Qt::OtherFocusReason);

        if (m_seat && m_seat->nativeHandle())
            m_seat->setKeyboardFocusSurface(nullptr);
    }

    // Assign new keyboard focus surface and update interaction metadata
    m_keyboardFocusSurface = surface;
    if (surface && m_seat && m_seat->nativeHandle()) {
        surface->setProperty("lastInteractingSeat", QVariant::fromValue(m_seat));
        surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());

        surface->setFocus(true, Qt::OtherFocusReason);
        m_seat->setKeyboardFocusSurface(surface->surface());
    }
}

void SeatSurfaceManager::beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges)
{
    if (m_moveResizeState.surface)
        endMoveResize();

    if (surface->surfaceState() != SurfaceWrapper::State::Normal ||
        surface->isAnimationRunning())
        return;

    m_moveResizeState.surface = surface;
    m_moveResizeState.edges = edges;
    m_moveResizeState.startGeometry = surface->geometry();
    m_moveResizeState.settingPositionFlag = false;

    surface->setXwaylandPositionFromSurface(false);
    surface->setPositionAutomatic(false);
}

void SeatSurfaceManager::doMoveResize(const QPointF &delta)
{
    if (!m_moveResizeState.surface)
        return;

    auto surface = m_moveResizeState.surface;

    if (m_moveResizeState.edges != Qt::Edges()) {
        QRectF geo = m_moveResizeState.startGeometry;

        if (m_moveResizeState.edges & Qt::LeftEdge)
            geo.setLeft(geo.left() + delta.x());
        if (m_moveResizeState.edges & Qt::TopEdge)
            geo.setTop(geo.top() + delta.y());
        if (m_moveResizeState.edges & Qt::RightEdge)
            geo.setRight(geo.right() + delta.x());
        if (m_moveResizeState.edges & Qt::BottomEdge)
            geo.setBottom(geo.bottom() + delta.y());

        QRectF alignedGeometry = surface->alignGeometryToPixelGrid(geo);
        surface->resize(alignedGeometry.size());
    } else {
        auto newPos = m_moveResizeState.startGeometry.topLeft() + delta;
        QPointF alignedPos = surface->alignToPixelGrid(newPos);
        surface->setPosition(alignedPos);
    }
}

void SeatSurfaceManager::endMoveResize()
{
    if (!m_moveResizeState.surface)
        return;

    auto surface = m_moveResizeState.surface;
    auto *sh = surface->shellSurface();
    if (sh && sh->isInitialized()) {
        // Mark resize operation as complete
        surface->shellSurface()->setResizeing(false);

        // Ensure window is still visible on screen after move/resize
        if (m_rootContainer) {
            m_rootContainer->ensureSurfaceNormalPositionValid(surface);
        }

        surface->setXwaylandPositionFromSurface(true);
    }

    // Clear state and notify
    m_moveResizeState.surface = nullptr;
    m_moveResizeState.edges = Qt::Edges();
    m_moveResizeState.startGeometry = QRectF();
    m_moveResizeState.initialPosition = QPointF();

    Q_EMIT moveResizeChanged();
}

SurfaceWrapper *SeatSurfaceManager::moveResizeSurface() const
{
    return m_moveResizeState.surface;
}

void SeatSurfaceManager::cancelMoveResize(SurfaceWrapper *surface)
{
    // Only cancel if this surface is the one being moved/resized
    if (m_moveResizeState.surface != surface)
        return;
    endMoveResize();
}

bool SeatSurfaceManager::shouldHandleShortcuts() const
{
    // Policy: Only primary seat handles shortcuts
    // The primary seat is the first seat in seatManager's list

    auto *helper = Helper::instance();
    if (!helper || !helper->seatManager())
        return false;

    const auto &seats = helper->seatManager()->seats();
    if (seats.isEmpty())
        return false;

    // In multi-seat environment, the fallback seat should handle shortcuts for backwards compatibility,
    // or the primary seat designated by the user. If no fallback is defined, use the first seat.
    if (auto *fallback = helper->seatManager()->fallbackSeat()) {
        return m_seat == fallback;
    }
    return m_seat == seats.first();
}

void SeatSurfaceManager::setMetaKeyPressed(bool pressed)
{
    if (m_metaKeyPressed == pressed)
        return;
    m_metaKeyPressed = pressed;
}

void SeatSurfaceManager::surfaceDestroyed(SurfaceWrapper *surface)
{
    if (m_moveResizeState.surface == surface) {
        endMoveResize();
    }

    if (m_activatedSurface == surface) {
        setActivatedSurface(nullptr, Qt::OtherFocusReason);
    }

    if (m_keyboardFocusSurface == surface) {
        setKeyboardFocusSurface(nullptr);
    }
}
