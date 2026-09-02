// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "seatsurfacemanager.h"

#include "rootsurfacecontainer.h"
#include "treelanduserconfig.hpp"
#include "common/treelandlogging.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "output/output.h"

#include <winputdevice.h>
#include <wcursor.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <wseat.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgpopupsurface.h>

#include <wlr_all.h>

#include <QDateTime>
#include <QTimer>

#include <algorithm>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
wlr_seat_keyboard_grab *keyboardGrabForPopup(wlr_xdg_popup *popup)
{
    if (!popup || !popup->seat || !popup->base || !popup->base->client
        || !popup->base->client->shell) {
        return nullptr;
    }

    auto *shell = popup->base->client->shell;
    wlr_xdg_popup_grab *popupGrab;
    wl_list_for_each(popupGrab, &shell->popup_grabs, link) {
        if (popupGrab->seat != popup->seat)
            continue;

        wlr_xdg_popup *member;
        wl_list_for_each(member, &popupGrab->popups, grab_link) {
            if (member == popup)
                return &popupGrab->keyboard_grab;
        }
    }

    return nullptr;
}
}

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
                    m_rootContainer->updateEdgeTilePreview(mr.detectedTileMode, out);
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

    const bool trackedPopup = isTrackedPopup(surface);
    QPointer<SurfaceWrapper> restoreTarget;
    if (trackedPopup && m_keyboardFocusSurface == surface) {
        restoreTarget = popupParentFocusTarget(surface, false);
        if (!restoreTarget && m_prePopupFocusSurface
            && m_prePopupFocusSurface->hasFocusCapability()) {
            restoreTarget = m_prePopupFocusSurface;
        }
    }

    m_popupFocusStack.removeIf([surface](const QPointer<SurfaceWrapper> &popup) {
        return !popup || popup == surface;
    });

    if (m_keyboardFocusSurface == surface) {
        setKeyboardFocusSurface(restoreTarget, Qt::ActiveWindowFocusReason);
        qCInfo(lcTlPopupFocus) << "Focused surface destroyed"
                               << "seat" << m_seat->name()
                               << "surface" << surface
                               << "trackedPopup" << trackedPopup
                               << "restoredFocus" << restoreTarget;
    }

    if (m_popupFocusStack.isEmpty() && !m_popupKeyboardGrab)
        m_prePopupFocusSurface.clear();
}

void SeatSurfaceManager::givePopupFocus(SurfaceWrapper *popupWrapper)
{
    Q_ASSERT(popupWrapper);
    auto *popupSurface = qobject_cast<WXdgPopupSurface *>(popupWrapper->shellSurface());
    if (!popupSurface)
        return;

    // Only give focus to popups that belong to our seat's active popup grab.
    auto *wlrPopup = popupSurface->handle();
    if (!wlrPopup || wlrPopup->seat != m_seat->handle())
        return;

    auto *seatHandle = m_seat->handle();
    auto *grab = seatHandle->keyboard_state.grab;
    auto *popupGrab = keyboardGrabForPopup(wlrPopup);
    if (!popupGrab || grab != popupGrab) {
        qCWarning(lcTlPopupFocus) << "Refusing popup focus without its exact keyboard grab"
                                  << "seat" << m_seat->name()
                                  << "popup" << popupWrapper
                                  << "expectedGrab" << popupGrab
                                  << "currentGrab" << grab;
        return;
    }

    if (m_popupKeyboardGrab && m_popupKeyboardGrab != popupGrab) {
        qCWarning(lcTlPopupFocus) << "Refusing popup focus while another structural grab is active"
                                  << "seat" << m_seat->name()
                                  << "popup" << popupWrapper
                                  << "trackedGrab" << m_popupKeyboardGrab
                                  << "popupGrab" << popupGrab;
        return;
    }

    if (!m_popupKeyboardGrab) {
        m_popupKeyboardGrab = popupGrab;
        m_prePopupFocusSurface = m_keyboardFocusSurface;
        ++m_popupTransitionSerial;
        qCInfo(lcTlPopupFocus) << "Popup keyboard grab tracked"
                               << "transition" << m_popupTransitionSerial
                               << "seat" << m_seat->name()
                               << "grab" << popupGrab
                               << "previousFocus" << m_prePopupFocusSurface;
    }

    m_popupFocusStack.removeIf([](const QPointer<SurfaceWrapper> &popup) {
        return popup.isNull();
    });
    m_popupFocusStack.removeAll(popupWrapper);
    m_popupFocusStack.append(popupWrapper);

    // Move keyboard focus to the popup surface directly.
    setKeyboardFocusSurface(popupWrapper, Qt::ActiveWindowFocusReason);

    qCInfo(lcTlPopupFocus) << "Popup received keyboard focus"
                           << "transition" << m_popupTransitionSerial
                           << "seat" << m_seat->name()
                           << "popup" << popupWrapper
                           << "parent" << popupWrapper->parentSurface()
                           << "depth" << m_popupFocusStack.size();
}

void SeatSurfaceManager::dismissPopups()
{
    m_popupFocusStack.removeIf([](const QPointer<SurfaceWrapper> &popup) {
        return popup.isNull();
    });
    if (!m_popupKeyboardGrab || m_popupFocusStack.isEmpty())
        return;

    auto *wrapper = m_popupFocusStack.constLast().data();
    auto *popup = wrapper
        ? qobject_cast<WXdgPopupSurface *>(wrapper->shellSurface())
        : nullptr;
    if (!popup || !popup->handle()) {
        qCWarning(lcTlPopupFocus) << "Unable to dismiss tracked popup"
                                  << "seat" << m_seat->name()
                                  << "wrapper" << wrapper
                                  << "grab" << m_popupKeyboardGrab;
        return;
    }

    qCInfo(lcTlPopupFocus) << "Dismissing concrete popup"
                           << "transition" << m_popupTransitionSerial
                           << "seat" << m_seat->name()
                           << "popup" << wrapper
                           << "grab" << m_popupKeyboardGrab;
    popup->close();
}

SurfaceWrapper *SeatSurfaceManager::popupParentFocusTarget(SurfaceWrapper *popup,
                                                           bool skipPopupParents) const
{
    auto *target = popup ? popup->parentSurface() : nullptr;
    while (target) {
        const bool isPopup = target->type() == SurfaceWrapper::Type::XdgPopup;
        if ((!skipPopupParents || !isPopup) && target->hasFocusCapability())
            return target;
        if (!isPopup)
            break;
        target = target->parentSurface();
    }
    return nullptr;
}

bool SeatSurfaceManager::isTrackedPopup(SurfaceWrapper *surface) const
{
    return std::any_of(m_popupFocusStack.cbegin(), m_popupFocusStack.cend(),
                       [surface](const QPointer<SurfaceWrapper> &popup) {
                           return popup == surface;
                       });
}

void SeatSurfaceManager::onKeyboardGrabBegin(wlr_seat_keyboard_grab *grab)
{
    if (!m_popupKeyboardGrab || grab == m_popupKeyboardGrab)
        return;

    auto *oldGrab = m_popupKeyboardGrab;
    m_popupKeyboardGrab = nullptr;
    m_popupFocusStack.clear();
    m_prePopupFocusSurface.clear();
    ++m_popupTransitionSerial;

    // wlroots replaces keyboard_state.grab directly and does not emit an end
    // event for the displaced grab. Forget the old popup state immediately;
    // the new grab owns focus policy from this point onward.
    qCInfo(lcTlPopupFocus) << "Popup keyboard grab replaced"
                           << "transition" << m_popupTransitionSerial
                           << "seat" << m_seat->name()
                           << "oldGrab" << oldGrab
                           << "newGrab" << grab
                           << "currentFocus" << m_keyboardFocusSurface;
}

void SeatSurfaceManager::onKeyboardGrabEnd(wlr_seat_keyboard_grab *grab)
{
    if (!m_popupKeyboardGrab || grab != m_popupKeyboardGrab) {
        qCDebug(lcTlPopupFocus) << "Ignoring unrelated keyboard grab end"
                                << "seat" << m_seat->name()
                                << "endedGrab" << grab
                                << "popupGrab" << m_popupKeyboardGrab;
        return;
    }

    QPointer<SurfaceWrapper> currentFocus = m_keyboardFocusSurface;
    QPointer<SurfaceWrapper> restoreTarget;
    const bool popupStillFocused = currentFocus && isTrackedPopup(currentFocus);
    if (popupStillFocused)
        restoreTarget = popupParentFocusTarget(currentFocus, true);
    if (!restoreTarget && m_prePopupFocusSurface
        && m_prePopupFocusSurface->hasFocusCapability()) {
        restoreTarget = m_prePopupFocusSurface;
    }

    m_popupKeyboardGrab = nullptr;
    m_popupFocusStack.clear();
    m_prePopupFocusSurface.clear();
    ++m_popupTransitionSerial;

    if (popupStillFocused)
        setKeyboardFocusSurface(restoreTarget, Qt::ActiveWindowFocusReason);

    qCInfo(lcTlPopupFocus) << "Popup keyboard grab ended"
                           << "transition" << m_popupTransitionSerial
                           << "seat" << m_seat->name()
                           << "endedGrab" << grab
                           << "previousFocus" << currentFocus
                           << "restoredFocus" << restoreTarget
                           << "activationUnchanged" << m_activatedSurface;
}
