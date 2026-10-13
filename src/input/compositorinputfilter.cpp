// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "compositorinputfilter.h"
#include "core/waylandserver.h"
#include "core/focuscontroller.h"
#include "idle/idlemanager.h"
#include "capture/capturemanager.h"
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "output/outputmanager.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "inputdevice.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/keyboard-shortcuts-inhibit/keyboardshortcutsinhibitmanager.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/active-notify/activenotifymanagerinterfacev1.h"
#include "common/treelandlogging.h"

#include <wrelativepointermanagerv1.h>
#include <wserver.h>
#include <wbackend.h>
#include <wcursor.h>
#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>
#include <wxdgpopupsurface.h>

#include <wlr_all.h>

#include <QScopeGuard>
#include <QMouseEvent>
#include <QKeySequence>

CompositorInputFilter::CompositorInputFilter(WaylandServer *server,
    FocusController *focus, IdleManager *idleManager, CaptureManager *captureManager,
    OutputManager *outputManager)
    : WSeatEventFilter(server)
    , m_server(server)
    , m_focus(focus)
    , m_idleManager(idleManager)
    , m_captureManager(captureManager)
    , m_outputManager(outputManager)
{}

void CompositorInputFilter::setShortcutProtocols(ShortcutManagerV2 *shortcuts,
    KeyboardShortcutsInhibitManagerV1 *inhibitor)
{
    m_shortcutManager = shortcuts;
    m_keyboardShortcutsInhibitManagerV1 = inhibitor;
}

bool CompositorInputFilter::beforeDisposeEvent(WSeat *seat, QWindow *targetWindow, QInputEvent *event)
{
    if (!m_server->isAvailable() || !m_server->window() || !m_server->backend()) {
        return false;
    }

    if (Q_UNLIKELY(!targetWindow || !event)) {
        return false;
    }

    if (event->isInputEvent()) {
        m_idleManager->notifyActivity(seat);

        // Only wake outputs disabled through output-power management.
        m_outputManager->wakePoweredOffOutputs();
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        const auto modifiers = kevent->modifiers();
        const auto ctrlAlt = Qt::ControlModifier | Qt::AltModifier;
        if ((modifiers & ctrlAlt) == ctrlAlt) {
            const auto key = kevent->key();
            if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
                const int vtnr = key - Qt::Key_F1 + 1;
                const bool sessionActive = m_server->backend()->isSessionActive();
                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut received"
                                        << vtnr << "sessionActive" << sessionActive;
                if (!sessionActive) {
                    return true;
                }

                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut requested" << vtnr;
                wlr_session_change_vt(m_server->backend()->session(), vtnr);
                return true;
            }
        }
    }

    WSeat *targetSeat = seat;
    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            targetSeat = m_server->seatManager()->getSeatForDevice(device);
            if (!targetSeat) {
                qCWarning(lcTlCore) << "Device has no associated seat, using default seat";
                targetSeat = seat;
            }
        }
    }

    if (targetSeat && targetSeat != seat) {
        return false;
    }
    m_currentEventSeat = targetSeat;
    [[maybe_unused]] auto clearEventSeat = qScopeGuard([this] { m_currentEventSeat = nullptr; });

    // Clear the record on release before the early returns below can bypass the
    // tracked block, otherwise the entry would linger until WSeat::destroy().
    // Auto-repeats are skipped: only the real release ends the press.
    if (seat == m_server->seat() && event->type() == QEvent::KeyRelease) {
        const auto *kevent = static_cast<QKeyEvent *>(event);
        const uint32_t evdevKeycode = kevent->nativeVirtualKey();
        if (!kevent->isAutoRepeat() && seat->isKeyFiltered(evdevKeycode))
            seat->removeFilteredKey(evdevKeycode);
    }

    // Dismiss the popup grab when the user presses a button outside the popup
    // (e.g. on the desktop or another client). wlroots' xdg popup keyboard grab
    // redirects keyboard focus back to the popup, so it must be ended explicitly.
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto *seatContainer = m_server->rootContainer()->getSeatContainer(targetSeat)) {
            if (seatContainer->hasPopupGrab()) {
                auto *focused = targetSeat->handle()->pointer_state.focused_surface;
                bool clickOnPopup = false;
                if (focused) {
                    if (auto *wSurface = WSurface::fromHandle(focused)) {
                        if (auto *wrapper = m_server->rootContainer()->getSurface(wSurface))
                            clickOnPopup = (wrapper->type() == SurfaceWrapper::Type::XdgPopup);
                    }
                }
                // seat->drag is non-null during an active DnD drag, which also
                // installs a keyboard grab; never end that grab here.
                if (!clickOnPopup && !targetSeat->handle()->drag)
                    seatContainer->dismissPopups();
            }
        }
    }
    if (seat == m_server->seat()) {
        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);
            switch (kevent->key()) {
            case Qt::Key_Meta:
            case Qt::Key_Super_L:
            case Qt::Key_Super_R:
                if (auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(true);
                break;
            default:
                if (auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(false);
                break;
            }
        }

        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);

#ifndef QT_NO_DEBUG
            if (QKeySequence(kevent->keyCombination()) ==
                QKeySequence(Qt::MetaModifier | Qt::Key_F12)) {
                std::terminate();
            }
            // The debug view shortcut should always handled first
            if (QKeySequence(kevent->keyCombination())
                == QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier | Qt::Key_F11)) {
                if (m_server->toggleDebugMenuBar())
                    return true;
            }
#endif

            if (m_captureManager->isSelecting()) {
                if (event->modifiers() == Qt::NoModifier && kevent->key() == Qt::Key_Escape)
                    m_captureManager->cancelSelection();
            }
        }

        if (event->type() == QEvent::KeyRelease && !m_captureManager->isSelecting()) {
            auto kevent = static_cast<QKeyEvent *>(event);
            const int key = kevent->key();
            if (key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Shift
                || key == Qt::Key_Meta) {
                Q_EMIT modifierKeyReleased(kevent);
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        handleLeftButtonStateChanged(event);
    }

    if (event->type() == QEvent::Wheel) {
        handleWhellValueChanged(event);
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    if (m_server->currentMode() != WaylandServer::CurrentMode::LockScreen)
        doGesture(event);

    // Per-seat move/resize handling
    const auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat);
    if (seatContainer && seatContainer->moveResizeState().surface) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            const auto &moveResizeState = seatContainer->moveResizeState();
            auto ownsOutput = moveResizeState.surface->ownsOutput();
            if (!ownsOutput) {
                m_server->rootContainer()->endMoveResizeForSeat(seat);
                return false;
            }

            auto increment_pos = ev->globalPosition() - moveResizeState.initialPosition;
            m_server->rootContainer()->doMoveResizeForSeat(seat, increment_pos);
            // Edge-tiling detection during move (resize is not tiled).
            if (moveResizeState.edges == Qt::Edges())
                m_server->rootContainer()->detectEdgeTilingForSeat(seat);

            return true;
        } else if (event->type() == QEvent::KeyPress
                   && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_server->rootContainer()->cancelMoveResizeForSeat(seat);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_server->rootContainer()->endMoveResizeForSeat(seat);
        }
    }

    // Suppress compositor shortcuts when a keyboard shortcuts inhibitor is active
    if (m_server->currentMode() == WaylandServer::CurrentMode::Normal) {
        auto *focusSurface = seat->keyboardFocusSurface();
        if (focusSurface
            && m_keyboardShortcutsInhibitManagerV1->isInhibited(seat->handle(),
                                                                focusSurface->handle()))
            return false;
    }

    // Capture mode: intercept key events before dispatchKeyEvent
    if (m_shortcutManager->isCaptureActive() && m_shortcutManager->tryHandleCaptureEvent(seat, event))
        return true;

    if (seat == m_server->seat() && !m_captureManager->isSelecting() && m_server->currentMode() != WaylandServer::CurrentMode::LockScreen
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto kevent = static_cast<QKeyEvent *>(event);
        auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat);

        // Meta: consume press as modifier; suppress release when used in combo
        if (kevent->key() == Qt::Key_Meta || kevent->key() == Qt::Key_Super_L || kevent->key() == Qt::Key_Super_R) {
            if (kevent->type() == QEvent::KeyPress) {
                return true;
            }
            if (kevent->type() == QEvent::KeyRelease && seatContainer && !seatContainer->metaKeyPressed()) {
                return false;
            }
        }

        auto *controller = m_shortcutManager->controller();
        const uint32_t evdevKeycode = kevent->nativeVirtualKey();
        const bool isAutoRepeat = kevent->isAutoRepeat();

        if (!isAutoRepeat && kevent->type() == QEvent::KeyPress
            && controller->matchesShortcut(kevent))
            seat->addFilteredKey(evdevKeycode);

        if (controller->dispatchKeyEvent(kevent))
            return true;
    }

    return false;
}

bool CompositorInputFilter::afterHandleEvent([[maybe_unused]] WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (!m_server->isAvailable() || !m_server->window() || !m_server->backend())
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_server->rootContainer()->getSurface(watched);
        WSeat *eventSeat = m_server->getSeatForEvent(event);

        if (eventSeat && surface) {
            m_focus->updateSurfaceSeatInteraction(surface, eventSeat);
            m_focus->activateSurface(surface, Qt::MouseFocusReason, eventSeat, true);
        }
    }

    return false;
}

bool CompositorInputFilter::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (!m_server->isAvailable() || !m_server->window() || !m_server->backend())
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        WSeat *eventSeat = m_server->getSeatForEvent(event);
        m_focus->activateSurface(nullptr, Qt::OtherFocusReason, eventSeat, true);
    }

    return false;
}

bool CompositorInputFilter::doGesture(QInputEvent *event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto e = static_cast<WGestureEvent *>(event);
        switch (e->gestureType()) {
        case Qt::BeginNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeStart(e->fingerCount());

            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    InputDevice::instance()->processSwipeCancel();
                else
                    InputDevice::instance()->processSwipeEnd();
            }
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldEnd();
            break;
        case Qt::PanNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeUpdate(e->delta());
        case Qt::ZoomNativeGesture:
        case Qt::SmartZoomNativeGesture:
        case Qt::RotateNativeGesture:
        case Qt::SwipeNativeGesture:
        default:
            break;
        }
    }
    return false;
}

void CompositorInputFilter::handleLeftButtonStateChanged(const QInputEvent *event)
{
    Q_ASSERT(m_server->seat());
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_server->seat();
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            DDEActiveInterface::sendActiveIn(DDEActiveInterface::Mouse, m_server->seat());
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Active,
                                                seat);
        } else {
            DDEActiveInterface::sendActiveOut(DDEActiveInterface::Mouse, m_server->seat());
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Inactive,
                                                seat);
        }
    }
}

void CompositorInputFilter::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_server->seat());
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_server->seat();
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        DDEActiveInterface::sendActiveOut(DDEActiveInterface::Wheel, m_server->seat());
        ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Wheel,
                                            ActiveNotifyV1::Inactive,
                                            seat);
    }
    if (delta.x() + delta.y() > 0) {
        DDEActiveInterface::sendActiveIn(DDEActiveInterface::Wheel, m_server->seat());
        ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Wheel,
                                            ActiveNotifyV1::Active,
                                            seat);
    }
}

void CompositorInputFilter::installRelativePointer(WServer *server, SeatManager *seatManager)
{
    m_relativePointerManager = server->attach<WRelativePointerManagerV1>();
    connect(seatManager, &SeatManager::seatAdded, this, &CompositorInputFilter::trackSeat);
}

void CompositorInputFilter::trackSeat(WSeat *seat)
{
    connect(seat, &WSeat::relativePointerMotion, this,
            [this, seat](uint32_t ts, QPointF delta, QPointF unaccelerated) {
                m_relativePointerManager->sendRelativeMotion(seat, ts, delta, unaccelerated);
            });
}
