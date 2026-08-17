// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "seatsurfacemanager.h"

#include "rootsurfacecontainer.h"
#include "treelanduserconfig.hpp"
#include "common/treelandlogging.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "core/shellhandler.h"
#include "output/output.h"

#include <winputdevice.h>
#include <wcursor.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <wseat.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgpopupsurface.h>

#include <WInputMethodHelper>

#include <wlr_all.h>

#include <QDateTime>
#include <QTimer>

WAYLIB_SERVER_USE_NAMESPACE

SeatSurfaceManager::SeatSurfaceManager(WSeat *seat, RootSurfaceContainer *parent)
    : QObject(parent)
    , m_seat(seat)
    , m_rootContainer(parent)
{
    Q_ASSERT(seat);
    Q_ASSERT(parent);

    auto *seatHandle = seat->handle();
    keyboardGrabBeginListener.init(&seatHandle->events.keyboard_grab_begin,
                                   this,
                                   &SeatSurfaceManager::onKeyboardGrabBegin);
    keyboardGrabEndListener.init(&seatHandle->events.keyboard_grab_end,
                                 this,
                                 &SeatSurfaceManager::onKeyboardGrabEnd);
}

SeatSurfaceManager::~SeatSurfaceManager()
{
    // Drop native listeners before ~WScopedListener runs: on seat removal the
    // seat is detached/deleted before this object's deleteLater, and
    // wlr_seat_destroy asserts the keyboard_grab listener lists are empty.
    // (QObject::connect on qw_seat used to auto-disconnect.)
    keyboardGrabBeginListener.disconnect();
    keyboardGrabEndListener.disconnect();
    endMoveResize();
}

void SeatSurfaceManager::setActivatedSurface(SurfaceWrapper *surface, Qt::FocusReason reason)
{
    Q_UNUSED(reason);
    if (m_activatedSurface == surface)
        return;

    if (m_activatedSurface) {
        disconnect(m_activatedSurface,
                   &SurfaceWrapper::hasFocusCapabilityChanged,
                   this,
                   &SeatSurfaceManager::onActivatedSurfaceFocusCapabilityChanged);
    }

    m_activatedSurface = surface;

    if (m_activatedSurface) {
        connect(m_activatedSurface,
                &SurfaceWrapper::hasFocusCapabilityChanged,
                this,
                &SeatSurfaceManager::onActivatedSurfaceFocusCapabilityChanged);
    }

    Q_EMIT activatedSurfaceChanged(surface);
}

void SeatSurfaceManager::onActivatedSurfaceFocusCapabilityChanged()
{
    if (!m_activatedSurface)
        return;

    auto *helper = Helper::instance();
    if (!helper)
        return;

    // While showing the desktop, keyboard focus is on the desktop layer, not on the
    // (hidden) activated surface; a focus-capability change of the activated
    // surface must not yank keyboard focus back to the window.
    if (helper->showDesktopState() == WindowManagementInterfaceV1::DesktopState::Show)
        return;

    if (m_activatedSurface->hasFocusCapability()) {
        helper->requestKeyboardFocus(m_activatedSurface, Qt::ActiveWindowFocusReason, m_seat);
    } else {
        helper->requestKeyboardFocus(nullptr, Qt::ActiveWindowFocusReason, m_seat);
    }
}

void SeatSurfaceManager::setKeyboardFocusSurface(SurfaceWrapper *surface, Qt::FocusReason reason)
{
    if (m_keyboardFocusSurface == surface)
        return;
    Q_ASSERT(m_seat && m_seat->handle());

    auto *oldSurface = m_keyboardFocusSurface;

    // Only check priority when transferring focus between two surfaces.
    // Clearing focus to nullptr (e.g. surfaceDestroyed) must always be allowed.
    if (oldSurface && surface) {
        int oldSurfacePriority = oldSurface->shellSurface() ? oldSurface->shellSurface()->keyboardFocusPriority() : 0;
        int newSurfacePriority = surface->shellSurface() ? surface->shellSurface()->keyboardFocusPriority() : 0;
        if (oldSurfacePriority > newSurfacePriority) {
            qCDebug(lcTlShell) << "Keyboard focus rejected: current surface priority"
                               << oldSurfacePriority
                               << "> new surface priority"
                               << newSurfacePriority;
            return;
        }
    }

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

            const auto seats = seatManager->seats();
            for (auto *otherSeat : seats) {
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
    }

    // Assign new keyboard focus surface and update interaction metadata
    m_keyboardFocusSurface = surface;
    m_seat->setKeyboardFocusSurface(surface ? surface->surface() : nullptr);

    if (surface) {
        surface->setFocus(true, reason);
        surface->setProperty("lastInteractingSeat", QVariant::fromValue(m_seat));
        surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
    }
}

void SeatSurfaceManager::restoreShowDesktopFocus()
{
    if (!m_activatedSurface || !m_activatedSurface->hasFocusCapability())
        return;

    // The seat may still be in transition (hotplug): without a container the
    // subsequent keyboard focus request would assert.
    if (!m_rootContainer || !m_rootContainer->getSeatContainer(m_seat))
        return;

    setKeyboardFocusSurface(m_activatedSurface, Qt::OtherFocusReason);
}

void SeatSurfaceManager::beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges)
{
    if (m_moveResizeState.surface)
        endMoveResize();
    // Move of a tiled/maximized window: instantly de-tile to Normal BEFORE
    // recording startGeometry, so startGeometry captures normalGeometry and no
    // state-change animation contends with doMoveResize's setPosition.
    if (edges == Qt::Edges()) {
        const auto st = surface->surfaceState();
        if (st == SurfaceWrapper::State::Tiling || st == SurfaceWrapper::State::Maximized)
            surface->setSurfaceStateDirectly(SurfaceWrapper::State::Normal);
    }

    if (surface->surfaceState() != SurfaceWrapper::State::Normal ||
        surface->isAnimationRunning())
        return;

    m_moveResizeState.surface = surface;
    m_moveResizeState.edges = edges;
    m_moveResizeState.startGeometry = surface->geometry();
    m_moveResizeState.settingPositionFlag = false;
    m_moveResizeState.detectedTileMode = SurfaceWrapper::TileMode::None;
    m_moveResizeState.edgeTilePreviewActive = false;
    m_moveResizeState.edgeTileInnerBorder = false;
    m_moveResizeState.detectedTileOutput = nullptr;

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
    stopEdgeTileDelay();
    if (!m_moveResizeState.surface)
        return;

    auto surface = m_moveResizeState.surface;
    const auto detectedMode = m_moveResizeState.detectedTileMode;
    const bool previewActive = m_moveResizeState.edgeTilePreviewActive;

    // Clear state first so filterSurfaceStateChange won't intercept the
    // subsequent setSurfaceState(Tiling) issued by SurfaceWrapper::applyTileMode.
    m_moveResizeState.surface = nullptr;
    m_moveResizeState.edges = Qt::Edges();
    m_moveResizeState.startGeometry = QRectF();
    m_moveResizeState.initialPosition = QPointF();
    m_moveResizeState.settingPositionFlag = false;
    m_moveResizeState.detectedTileMode = SurfaceWrapper::TileMode::None;
    m_moveResizeState.edgeTilePreviewActive = false;
    m_moveResizeState.edgeTileInnerBorder = false;
    m_moveResizeState.detectedTileOutput = nullptr;

    auto *sh = surface->shellSurface();
    if (sh && sh->isInitialized()) {
        // Mark resize operation as complete
        surface->shellSurface()->setResizeing(false);
        surface->setXwaylandPositionFromSurface(true);
    }
    if (!previewActive || detectedMode == SurfaceWrapper::TileMode::None) {
        // Ensure window is still visible on screen after a plain move/resize.
        if (m_rootContainer)
            m_rootContainer->ensureSurfaceNormalPositionValid(surface);
    } else {
        Output *out = nullptr;
        if (m_rootContainer && m_seat && m_seat->cursor())
            out = m_rootContainer->outputAt(m_seat->cursor()->position());
        surface->applyTileMode(detectedMode, out);
    }

    Q_EMIT moveResizeChanged();
}

SurfaceWrapper *SeatSurfaceManager::moveResizeSurface() const
{
    return m_moveResizeState.surface;
}

void SeatSurfaceManager::cancelMoveResize()
{
    if (!m_moveResizeState.surface)
        return;

    auto surface = m_moveResizeState.surface;
    auto startGeo = m_moveResizeState.startGeometry;
    // Cancel discards any edge-tiling detected during the move: restore the
    // original (normal) geometry captured at beginMoveResize.
    m_moveResizeState.detectedTileMode = SurfaceWrapper::TileMode::None;
    m_moveResizeState.detectedTileOutput = nullptr;
    m_moveResizeState.edgeTilePreviewActive = false;
    m_moveResizeState.edgeTileInnerBorder = false;

    // Restore original geometry before ending
    if (m_moveResizeState.edges != Qt::Edges()) {
        surface->resize(surface->alignGeometryToPixelGrid(startGeo).size());
    } else {
        surface->setPosition(surface->alignToPixelGrid(startGeo.topLeft()));
    }

    endMoveResize();
}

void SeatSurfaceManager::cancelMoveResize(SurfaceWrapper *surface)
{
    // Only cancel if this surface is the one being moved/resized
    if (m_moveResizeState.surface != surface)
        return;
    endMoveResize();
}

void SeatSurfaceManager::startEdgeTileDelay()
{
    if (!m_edgeTileDelayTimer) {
        m_edgeTileDelayTimer = new QTimer(this);
        m_edgeTileDelayTimer->setSingleShot(true);
        connect(m_edgeTileDelayTimer, &QTimer::timeout, this, [this]() {
            auto &mr = m_moveResizeState;
            if (mr.surface && mr.edges == Qt::Edges()
                && mr.detectedTileMode != SurfaceWrapper::TileMode::None) {
                mr.edgeTilePreviewActive = true;
                Output *out = nullptr;
                if (m_rootContainer && m_seat && m_seat->cursor())
                    out = m_rootContainer->outputAt(m_seat->cursor()->position());
                if (m_rootContainer)
                    m_rootContainer->updateEdgeTilePreview(mr.detectedTileMode, out, m_seat);
            }
        });
    }
    auto *helper = Helper::instance();
    auto *cfg = helper ? helper->config() : nullptr;
    m_edgeTileDelayTimer->setInterval(cfg ? cfg->edgeInnerDelayMs() : 250);
    m_edgeTileDelayTimer->start();
}

void SeatSurfaceManager::stopEdgeTileDelay()
{
    if (m_edgeTileDelayTimer)
        m_edgeTileDelayTimer->stop();
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

void SeatSurfaceManager::givePopupFocus(SurfaceWrapper *popupWrapper)
{
    if (!m_hasPopupGrab)
        return;

    Q_ASSERT(popupWrapper);
    auto *popupSurface = qobject_cast<WXdgPopupSurface *>(popupWrapper->shellSurface());
    if (!popupSurface)
        return;

    // Only give focus to popups that belong to our seat's active popup grab.
    auto *wlrPopup = popupSurface->handle();
    if (!wlrPopup || wlrPopup->seat != m_seat->handle())
        return;

    // Move keyboard focus to the popup surface directly.
    setKeyboardFocusSurface(popupWrapper, Qt::ActiveWindowFocusReason);

    qCDebug(lcTlPopupFocus) << "Moved keyboard focus to popup surface:" << popupWrapper;
}

void SeatSurfaceManager::dismissPopups()
{
    if (!m_hasPopupGrab)
        return;

    qCDebug(lcTlPopupFocus) << "Dismissing popup grab";
    wlr_seat_keyboard_end_grab(m_seat->handle());
}

void SeatSurfaceManager::onKeyboardGrabBegin()
{
    if (m_hasPopupGrab) {
        // Already tracking a popup grab; nested popups share the same flag.
        return;
    }

    auto *seatNative = m_seat->handle();
    auto *grab = seatNative->keyboard_state.grab;
    if (!grab) {
        qCWarning(lcTlPopupFocus) << "keyboard_state.grab is null";
        return;
    }

    // Detect IME keyboard grab:
    // WInputMethodHelper::handleNewKGV2 sets activeKeyboardGrab before
    // calling keyboard_start_grab, so it is already non-null when we get here.
    // Use isActiveKeyboardGrabOwner() to check if the seat's current grab
    // is the one installed by the IME helper.
    if (auto *imHelper = Helper::instance()->shellHandler()->inputMethodHelper()) {
        if (imHelper->isActiveKeyboardGrabOwner()) {
            qCDebug(lcTlPopupFocus) << "IME keyboard grab started (not popup)";
            return;
        }
    }

    m_hasPopupGrab = true;
    qCDebug(lcTlPopupFocus) << "Popup keyboard grab started";
}

void SeatSurfaceManager::onKeyboardGrabEnd()
{
    if (!m_hasPopupGrab)
        return;

    m_hasPopupGrab = false;

    qCDebug(lcTlPopupFocus) << "Popup keyboard grab ended, restoring focus to:"
                            << m_activatedSurface;

    // While showing the desktop, keyboard focus is on the desktop layer, not on the
    // (hidden) activated surface; do not yank it back to the window.
    if (auto *helper = Helper::instance()) {
        if (helper->showDesktopState() == WindowManagementInterfaceV1::DesktopState::Show)
            return;
    }

    if (m_activatedSurface && m_activatedSurface->hasFocusCapability()) {
        setKeyboardFocusSurface(m_activatedSurface, Qt::ActiveWindowFocusReason);
    }
}
