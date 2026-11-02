// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <utility>
#include "compositorinputfilter.h"
#include "inputcontext.h"
#include "idle/idlemanager.h"
#include "capture/capturemanager.h"
#include "surface/seatsurfacemanager.h"
#include "output/output.h"
#include "output/outputmanager.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "gesturemanager.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/keyboard-shortcuts-inhibit/keyboardshortcutsinhibitmanager.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/active-notify/activenotifymanagerinterfacev1.h"
#include "common/treelandlogging.h"

#include <wrelativepointermanagerv1.h>
#include <wserver.h>
#include <wcursor.h>
#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>
#include <wxdgpopupsurface.h>

#include <wlr_all.h>

#include <QScopeGuard>
#include <QMouseEvent>
#include <QKeySequence>

CompositorInputFilter::CompositorInputFilter(InputContext *context, QObject *parent,
    IdleManager *idleManager, CaptureManager *captureManager, OutputManager *outputManager)
    : WSeatEventFilter(parent)
    , m_context(context)
    , m_idleManager(idleManager)
    , m_captureManager(captureManager)
    , m_outputManager(outputManager)
{}

void CompositorInputFilter::setShortcutProtocols(ShortcutManagerV3 *shortcuts,
    KeyboardShortcutsInhibitManagerV1 *inhibitor)
{
    m_shortcutManager = shortcuts;
    m_keyboardShortcutsInhibitManagerV1 = inhibitor;
}

bool CompositorInputFilter::beforeDisposeEvent(WSeat *seat, QWindow *targetWindow, QInputEvent *event)
{
    if (!acceptsEvent(seat, event) || (m_admission && !m_admission(seat, event)))
        return true;
    if (!m_context->available()) {
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
                m_context->switchVirtualTerminal(vtnr);
                return true;
            }
        }
    }

    WSeat *targetSeat = seat;
    m_currentEventSeat = targetSeat;
    [[maybe_unused]] auto clearEventSeat = qScopeGuard([this] { m_currentEventSeat = nullptr; });

    // Clear the record on release before the early returns below can bypass the
    // tracked block, otherwise the entry would linger until WSeat::destroy().
    // Auto-repeats are skipped: only the real release ends the press.
    if (seat == m_context->primarySeat() && event->type() == QEvent::KeyRelease) {
        const auto *kevent = static_cast<QKeyEvent *>(event);
        const uint32_t evdevKeycode = kevent->nativeVirtualKey();
        if (!kevent->isAutoRepeat() && seat->isKeyFiltered(evdevKeycode))
            seat->removeFilteredKey(evdevKeycode);
    }

    // Dismiss the popup grab when the user presses a button outside the popup
    // (e.g. on the desktop or another client). wlroots' xdg popup keyboard grab
    // redirects keyboard focus back to the popup, so it must be ended explicitly.
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto *seatContainer = m_context->seatContainer(targetSeat)) {
            if (seatContainer->hasPopupGrab()) {
                auto *focused = targetSeat->handle()->pointer_state.focused_surface;
                bool clickOnPopup = false;
                if (focused) {
                    if (auto *wSurface = WSurface::fromHandle(focused)) {
                        if (auto *wrapper = m_context->surface(wSurface))
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
    if (seat == m_context->primarySeat()) {
        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);
            switch (kevent->key()) {
            case Qt::Key_Meta:
            case Qt::Key_Super_L:
            case Qt::Key_Super_R:
                if (auto *seatContainer = m_context->seatContainer(seat))
                    seatContainer->setMetaKeyPressed(true);
                break;
            default:
                if (auto *seatContainer = m_context->seatContainer(seat))
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
                if (m_context->toggleDebugView())
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

    if (m_gestures && !m_context->locked())
        doGesture(event);

    // Per-seat move/resize handling
    auto *seatContainer = m_context->seatContainer(seat);
    if (seatContainer && seatContainer->moveResizeState().surface) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            const auto &moveResizeState = seatContainer->moveResizeState();
            auto ownsOutput = moveResizeState.surface->ownsOutput();
            if (!ownsOutput) {
                m_context->endMoveResize(seat);
                return false;
            }

            auto increment_pos = ev->globalPosition() - moveResizeState.initialPosition;
            m_context->moveResize(seat, increment_pos);
            // Edge-tiling detection during move (resize is not tiled).
            if (moveResizeState.edges == Qt::Edges())
                m_context->detectEdgeTiling(seat);

            return true;
        } else if (event->type() == QEvent::KeyPress
                   && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_context->cancelMoveResize(seat);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_context->endMoveResize(seat);
        }
    }

    // Suppress compositor shortcuts when a keyboard shortcuts inhibitor is active
    if (m_context->normal()) {
        auto *focusSurface = seat->keyboardFocusSurface();
        if (focusSurface && m_keyboardShortcutsInhibitManagerV1
            && m_keyboardShortcutsInhibitManagerV1->isInhibited(seat->handle(),
                                                                focusSurface->handle()))
            return false;
    }

    // Capture mode: intercept key events before dispatchKeyEvent
    if (m_shortcutManager && m_shortcutManager->isCaptureActive() && m_shortcutManager->tryHandleCaptureEvent(seat, event))
        return true;

    if (seat == m_context->primarySeat() && !m_captureManager->isSelecting() && !m_context->locked()
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto kevent = static_cast<QKeyEvent *>(event);
        auto *seatContainer = m_context->seatContainer(seat);

        // Meta: consume press as modifier; suppress release when used in combo
        if (kevent->key() == Qt::Key_Meta || kevent->key() == Qt::Key_Super_L || kevent->key() == Qt::Key_Super_R) {
            if (kevent->type() == QEvent::KeyPress) {
                return true;
            }
            if (kevent->type() == QEvent::KeyRelease && seatContainer && !seatContainer->metaKeyPressed()) {
                return false;
            }
        }

        auto *controller = m_controller;
        if (!controller)
            return false;
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

bool CompositorInputFilter::afterHandleEvent(WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (!acceptsEvent(seat, event))
        return true;

    if (!m_context->available())
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto *item = qobject_cast<WSurfaceItem *>(surfaceItem);
        auto *toplevelSurface = item ? item->shellSurface() : nullptr;
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        m_context->activateSurface(watched, seat, Qt::MouseFocusReason);
    }

    return false;
}

bool CompositorInputFilter::unacceptedEvent(WSeat *seat, QWindow *, QInputEvent *event)
{
    if (!acceptsEvent(seat, event))
        return true;
    if (!m_context->available())
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        m_context->activateSurface(nullptr, seat, Qt::OtherFocusReason);
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
                m_gestures->processSwipeStart(e->fingerCount());

            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                m_gestures->processHoldStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    m_gestures->processSwipeCancel();
                else
                    m_gestures->processSwipeEnd();
            }
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                m_gestures->processHoldEnd();
            break;
        case Qt::PanNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                m_gestures->processSwipeUpdate(e->delta());
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
    Q_ASSERT(m_context->primarySeat());
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_context->primarySeat();
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            DDEActiveInterface::sendActiveIn(DDEActiveInterface::Mouse, seat);
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Active,
                                                seat);
        } else {
            DDEActiveInterface::sendActiveOut(DDEActiveInterface::Mouse, seat);
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Inactive,
                                                seat);
        }
    }
}

void CompositorInputFilter::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_context->primarySeat());
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_context->primarySeat();
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        DDEActiveInterface::sendActiveOut(DDEActiveInterface::Wheel, seat);
        ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Wheel,
                                            ActiveNotifyV1::Inactive,
                                            seat);
    }
    if (delta.x() + delta.y() > 0) {
        DDEActiveInterface::sendActiveIn(DDEActiveInterface::Wheel, seat);
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
    disconnect(seat, &WSeat::relativePointerMotion, this, nullptr);
    connect(seat, &WSeat::relativePointerMotion, this,
            [this, seat](uint32_t ts, QPointF delta, QPointF unaccelerated) {
                if (!seat->cursor())
                    return;
                m_relativePointerManager->sendRelativeMotion(seat, ts, delta, unaccelerated);
            });
}

void CompositorInputFilter::setEnabled(bool enabled)
{
    m_enabled = enabled;
    m_currentEventSeat = nullptr;
}

void CompositorInputFilter::setAdmission(std::function<bool(WSeat *, QInputEvent *)> admission)
{
    m_admission = std::move(admission);
}

void CompositorInputFilter::setGestureManager(GestureManager *gestures) { m_gestures = gestures; }
void CompositorInputFilter::setController(ShortcutController *controller) { m_controller = controller; }

bool CompositorInputFilter::acceptsEvent(WSeat *seat, QInputEvent *event) const
{
    if (!m_enabled || !seat || !event || !m_context->available() || seat->eventFilter() != this)
        return false;
    if (event->device()) {
        if (auto *device = WInputDevice::from(event->device()))
            return device->seat() == seat;
    }
    return true;
}

bool CompositorInputFilter::beforeHandleEvent(WSeat *seat, WSurface *watched, QObject *,
                                             QObject *, QInputEvent *event)
{
    if (!acceptsEvent(seat, event))
        return true;
    if (watched && !seat->ownsClient(watched->waylandClient()))
        return true;
    return watched && !m_context->surface(watched);
}
