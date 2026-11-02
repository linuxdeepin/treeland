// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <utility>
#include <QPointer>
#include "shortcutmanager.h"

#include "common/treelandlogging.h"
#include "input/swipegesture.h"
#include "qwayland-server-treeland-shortcut-manager-unstable-v3.h"
#include "shortcutcontroller.h"

#include <wseat.h>
#include <wsurface.h>

#include <wlr_all.h>

#include <QKeyEvent>
#include <QKeySequence>
#include <QMetaEnum>

struct KeyShortcut {
    ShortcutController::KeyFlags keybindFlags;
    QString name;
    QString key;
    ShortcutAction action;
};

struct SwipeShortcut {
    QString name;
    uint finger;
    SwipeGesture::Direction direction;
    ShortcutAction action;
};

struct HoldShortcut {
    QString name;
    uint finger;
    ShortcutAction action;
};

// Returns true when the action value is one of the defined ShortcutAction
// enum values. Uses QMetaEnum instead of a range check so it stays correct
// even if the enum gains holes or is reordered in the future.
static bool isValidAction(ShortcutAction action)
{
    const auto meta = QMetaEnum::fromType<ShortcutAction>();
    return meta.valueToKey(static_cast<int>(action)) != nullptr;
}

// Forward declaration
class ShortcutManagerV3Private;

// Returns true if the key itself is a pure modifier key that can never be a valid
// shortcut on its own (Ctrl/Alt/Shift/Hyper/AltGr/lock keys).
//
// NOTE: Qt::Key_Meta, Qt::Key_Super_L, Qt::Key_Super_R are intentionally NOT listed —
// on Linux/Wayland the Win key maps to one of these, and pressing it alone IS a valid
// standalone shortcut (dde-daemon isGoodNoMods() explicitly includes XK_Super_L/R).
static bool isPureModifierKey(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
        return true;
    default:
        return false;
    }
}

class ShortcutCaptureV3 : public QtWaylandServer::treeland_shortcut_capture_v3
{
public:
    ShortcutCaptureV3(ShortcutManagerV3Private *manager,
                      wl_client *client,
                      uint32_t id,
                      int version)
        : QtWaylandServer::treeland_shortcut_capture_v3(client, id, version)
        , m_manager(manager)
        , m_pending(true)
    {
    }

    bool isPending() const
    {
        return m_pending;
    }

    void sendCaptured(const QString &key)
    {
        m_pending = false;
        send_captured(key);
    }

    void sendFailed(uint32_t reason)
    {
        m_pending = false;
        send_failed(reason);
    }

protected:
    // Defined after ShortcutManagerV3Private
    void destroy(Resource *resource) override;

    // Called both when the client sends destroy and when the client disconnects.
    // Must clean up m_pendingCapture to avoid a dangling pointer.
    void destroy_resource(Resource *resource) override;

private:
    ShortcutManagerV3Private *m_manager;
    bool m_pending;
};

static SwipeGesture::Direction toSwipeDirection(uint32_t direction)
{
    using Direction = QtWaylandServer::treeland_shortcut_manager_v3::direction;
    switch (direction) {
    case Direction::direction_down:
        return SwipeGesture::Direction::Down;
    case Direction::direction_left:
        return SwipeGesture::Direction::Left;
    case Direction::direction_up:
        return SwipeGesture::Direction::Up;
    case Direction::direction_right:
        return SwipeGesture::Direction::Right;
    default:
        return SwipeGesture::Direction::Invalid;
    }
}

class ShortcutManagerV3Private : public QtWaylandServer::treeland_shortcut_manager_v3
{
public:
    explicit ShortcutManagerV3Private(ShortcutManagerV3::SeatProvider seats);

    wl_global *global() const;

    void sendActivated(const QString &name, ShortcutController::KeyFlags keyFlags);
    void sendBindFailure(const QString &name, uint error);
    uint registerKey(const KeyShortcut &ks);
    uint registerSwipe(const SwipeShortcut &ss);
    uint registerHold(const HoldShortcut &hs);

    QPointer<ShortcutController> m_controller;
    Resource *m_owner = nullptr;
    const ShortcutManagerV3::SeatProvider m_seats;

    ShortcutCaptureV3 *m_pendingCapture = nullptr;
    // Seat whose keyboard focus was validated at capture start.
    // Events from other seats are ignored during capture.
    WSeat *m_pendingSeat = nullptr;
    Qt::Key m_drainKey = Qt::Key_unknown;
    WSeat *m_drainSeat = nullptr;
    // Target surface that requested the capture. Capture must fail once it loses focus.
    QPointer<WSurface> m_pendingSurface;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void acquire(Resource *resource) override;
    void bind_key(Resource *resource,
                  const QString &name,
                  const QString &key_sequence,
                  uint32_t flags,
                  uint32_t action) override;
    void bind_swipe_gesture(Resource *resource,
                            const QString &name,
                            uint32_t finger,
                            uint32_t direction,
                            uint32_t action) override;
    void bind_hold_gesture(Resource *resource,
                           const QString &name,
                           uint32_t finger,
                           uint32_t action) override;
    void unbind(Resource *resource, const QString &name) override;
    void capture_next_shortcut(Resource *resource,
                               struct ::wl_resource *surface,
                               struct ::wl_resource *seat,
                               uint32_t capture) override;

private:
    // Clears all capture-related state and returns the previously pending capture object.
    // The caller is responsible for sending a terminal event on the returned object.
    ShortcutCaptureV3 *resetCaptureState();

public:
    void cancelCapture();
    void onCaptureDestroyed(ShortcutCaptureV3 *c);
    bool tryHandleCaptureEvent(WSeat *seat, QInputEvent *event);
};

ShortcutManagerV3Private::ShortcutManagerV3Private(ShortcutManagerV3::SeatProvider seats)
    : m_seats(std::move(seats))
{
}

wl_global *ShortcutManagerV3Private::global() const
{
    return m_global;
}

uint ShortcutManagerV3Private::registerKey(const KeyShortcut &ks)
{
    if (!isValidAction(ks.action))
        return QtWaylandServer::treeland_shortcut_manager_v3::bind_error_invalid_argument;
    return m_controller->registerKey(ks.name, ks.key, ks.keybindFlags, ks.action);
}

uint ShortcutManagerV3Private::registerSwipe(const SwipeShortcut &ss)
{
    if (!isValidAction(ss.action))
        return QtWaylandServer::treeland_shortcut_manager_v3::bind_error_invalid_argument;
    return m_controller->registerSwipeGesture(ss.name, ss.finger, ss.direction, ss.action);
}

uint ShortcutManagerV3Private::registerHold(const HoldShortcut &hs)
{
    if (!isValidAction(hs.action))
        return QtWaylandServer::treeland_shortcut_manager_v3::bind_error_invalid_argument;
    return m_controller->registerHoldGesture(hs.name, hs.finger, hs.action);
}

void ShortcutManagerV3Private::sendActivated(const QString &name, ShortcutController::KeyFlags keyFlags)
{
    if (m_owner)
        send_activated(m_owner->handle, name, keyFlags.toInt());
}

void ShortcutManagerV3Private::sendBindFailure(const QString &name, uint error)
{
    if (m_owner)
        send_bind_failure(m_owner->handle, name, error);
}

void ShortcutManagerV3Private::destroy_resource(Resource *resource)
{
    if (m_owner == resource) {
        m_owner = nullptr;
        if (m_controller)
            m_controller->clear();
        cancelCapture();
    }
}

void ShortcutManagerV3Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShortcutManagerV3Private::acquire(Resource *resource)
{
    if (m_owner) {
        wl_resource_post_error(resource->handle,
                               error_occupied,
                               "Another client has already acquired the shortcut manager.");
        return;
    }
    m_controller->clear();
    m_owner = resource;
}

void ShortcutManagerV3Private::bind_key(Resource *resource,
                                        const QString &name,
                                        const QString &key_sequence,
                                        uint32_t flags,
                                        uint32_t action)
{
    if (m_owner != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    KeyShortcut ks{
        .keybindFlags = ShortcutController::KeyFlags::fromInt(flags),
        .name = name,
        .key = key_sequence,
        .action = static_cast<ShortcutAction>(action),
    };

    const uint status = registerKey(ks);
    if (status)
        sendBindFailure(name, status);
}

void ShortcutManagerV3Private::bind_swipe_gesture(Resource *resource,
                                                  const QString &name,
                                                  uint32_t finger,
                                                  uint32_t direction,
                                                  uint32_t action)
{
    if (m_owner != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    SwipeShortcut ss{
        .name = name,
        .finger = finger,
        .direction = toSwipeDirection(direction),
        .action = static_cast<ShortcutAction>(action),
    };

    const uint status = registerSwipe(ss);
    if (status)
        sendBindFailure(name, status);
}

void ShortcutManagerV3Private::bind_hold_gesture(Resource *resource,
                                                 const QString &name,
                                                 uint32_t finger,
                                                 uint32_t action)
{
    if (m_owner != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    HoldShortcut hs{
        .name = name,
        .finger = finger,
        .action = static_cast<ShortcutAction>(action),
    };

    const uint status = registerHold(hs);
    if (status)
        sendBindFailure(name, status);
}

void ShortcutManagerV3Private::unbind(Resource *resource, const QString &name)
{
    if (m_owner != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_controller->unregisterShortcut(name);
}

void ShortcutCaptureV3::destroy(Resource *resource)
{
    // destroy_resource() handles cleanup; just trigger it.
    wl_resource_destroy(resource->handle);
}

void ShortcutCaptureV3::destroy_resource(Resource *)
{
    if (m_pending)
        m_manager->onCaptureDestroyed(this);
    delete this;
}

void ShortcutManagerV3Private::capture_next_shortcut(Resource *resource,
                                                     struct ::wl_resource *surface,
                                                     struct ::wl_resource *seat_resource,
                                                     uint32_t capture)
{
    // Validate surface ownership: the surface must belong to the requesting client.
    if (wl_resource_get_client(surface) != wl_resource_get_client(resource->handle)) {
        wl_resource_post_error(resource->handle,
                               error_invalid_surface,
                               "Surface does not belong to the requesting client.");
        return;
    }

    // Create the capture resource.
    auto *captureObj =
        new ShortcutCaptureV3(this, resource->client(), capture, resource->version());

    // Check if another capture is already in progress.
    if (m_pendingCapture || m_drainKey != Qt::Key_unknown) {
        captureObj->sendFailed(ShortcutCaptureV3::failed_reason_busy);
        return;
    }

    // Resolve the target surface first so it can be used for focus-based seat lookup.
    auto *wlrSurface = wlr_surface_from_resource(surface);
    auto *wSurface = wlrSurface ? WSurface::fromHandle(wlrSurface) : nullptr;

    WSeat *requestedSeat = nullptr;
    if (seat_resource) {
        // Client specified a seat: resolve it directly.
        auto *wlrSeatClient = wlr_seat_client_from_resource(seat_resource);
        if (wlrSeatClient)
            requestedSeat = WSeat::fromHandle(wlrSeatClient->seat);
    } else if (m_seats) {
        // Resolve focus only within this protocol instance's seat domain.
        const auto seats = m_seats();
        for (auto *seat : seats) {
            if (seat->keyboardFocusSurface() == wSurface) {
                requestedSeat = seat;
                break;
            }
        }
    }

    if (requestedSeat && m_seats && !m_seats().contains(requestedSeat)) {
        captureObj->sendFailed(ShortcutCaptureV3::failed_reason_not_active);
        return;
    }

    // Validate surface focus / active state.
    auto *focusedSurface = requestedSeat ? requestedSeat->keyboardFocusSurface() : nullptr;
    if (!focusedSurface || focusedSurface != wSurface) {
        captureObj->sendFailed(ShortcutCaptureV3::failed_reason_not_active);
        return;
    }

    m_pendingCapture = captureObj;
    m_pendingSeat = requestedSeat;
    m_pendingSurface = wSurface;
}

void ShortcutManagerV3Private::onCaptureDestroyed(ShortcutCaptureV3 *c)
{
    if (m_pendingCapture == c)
        resetCaptureState();
}

ShortcutCaptureV3 *ShortcutManagerV3Private::resetCaptureState()
{
    auto *c = m_pendingCapture;
    m_pendingCapture = nullptr;
    m_pendingSeat = nullptr;
    m_pendingSurface.clear();
    return c;
}

// Capture event filter — called from CompositorInputFilter while a
// one-shot shortcut capture is in progress.
//
// Trigger semantics:
//   KeyPress (regular non-modifier) → TERMINATE capture immediately
//                                     (captured or failed).
//   KeyRelease (Meta/Super only)    → used for standalone Win key capture.
//
// Key categories:
//   pure modifier (Ctrl/Alt/Shift/…) — consumed on press; KeyRelease → always fail.
//   Win key (Super_L/R or Meta)      — consumed on press; KeyRelease → emit "Meta" if
//                                      no other modifier is held (any regular key pressed
//                                      while Win is held terminates capture on KeyPress).
//   all other keys                   — terminate on KeyPress via
//                                      ShortcutController::isValidShortcutCombination().
bool ShortcutManagerV3Private::tryHandleCaptureEvent(WSeat *seat, QInputEvent *event)
{
    // Drain the captured key's residual KeyRelease to avoid triggering
    // a newly bound shortcut with KeyRelease trigger semantics.
    if (m_drainKey != Qt::Key_unknown && seat == m_drainSeat
        && event->type() == QEvent::KeyRelease) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (static_cast<Qt::Key>(ke->key()) == m_drainKey) {
            m_drainKey = Qt::Key_unknown;
            m_drainSeat = nullptr;
            return true;
        }
    }

    if (!m_pendingCapture)
        return false;

    // Seat filter: ignore events from seats other than the one used to validate focus.
    if (seat != m_pendingSeat)
        return false;

    // If the requesting surface lost focus, abort capture but do not consume the
    // current event, so input can continue to the newly focused target.
    if (!m_pendingSurface || m_pendingSeat->keyboardFocusSurface() != m_pendingSurface) {
        resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_aborted);
        return false;
    }

    // Pointer button or wheel: cancel immediately.
    const auto type = event->type();
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease
        || type == QEvent::Wheel) {
        resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_interrupted);
        return true;
    }

    if (type != QEvent::KeyPress && type != QEvent::KeyRelease)
        return false;

    auto *kevent = static_cast<QKeyEvent *>(event);

    // Auto-repeat: consume silently without updating state.
    if (kevent->isAutoRepeat())
        return true;

    Qt::Key key = static_cast<Qt::Key>(kevent->key());

    if (type == QEvent::KeyPress) {
        if (isPureModifierKey(key)) {
            // Pure modifier (Ctrl/Alt/Shift/…): consume and keep waiting.
            // A previously stored candidate stays valid.
            return true;
        }

        if (key == Qt::Key_Super_L || key == Qt::Key_Super_R || key == Qt::Key_Meta) {
            // Win/Super key: at KeyPress time we cannot yet tell whether the user
            // intends it alone ("Meta") or combined with another key ("Meta+X").
            // Treat it like a modifier — consume and wait.  The KeyRelease path
            // below will emit "Meta" if no other key was pressed in between.
            return true;
        }

        // Regular non-modifier key: terminate capture immediately on KeyPress.
        if (ShortcutController::isValidShortcutCombination(kevent->keyCombination())) {
            // Arm drain before resetting (resetCaptureState clears m_pendingSeat).
            m_drainKey = key;
            m_drainSeat = m_pendingSeat;
            auto keyComb = ShortcutController::normalizeKeyCombination(kevent->keyCombination());
            auto captured = QKeySequence(keyComb).toString(QKeySequence::PortableText);
            resetCaptureState()->sendCaptured(captured);
        } else {
            // Invalid combo: fail immediately.
            resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_interrupted);
        }
    } else { // KeyRelease, only needed for modifier-only paths.
        if (isPureModifierKey(key)) {
            // Pure modifier released without any regular key press.
            resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_interrupted);
        } else if (key == Qt::Key_Super_L || key == Qt::Key_Super_R || key == Qt::Key_Meta) {
            // Win/Super is only valid when pressed alone (no other modifiers held).
            if ((kevent->modifiers() & ~Qt::MetaModifier) == Qt::NoModifier)
                resetCaptureState()->sendCaptured(QStringLiteral("Meta"));
            else
                resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_interrupted);
        } else {
            // With press-trigger semantics, regular non-modifier release should not
            // normally reach here. Be defensive and fail-safe if it does.
            resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_interrupted);
        }
    }
    return true;
}

ShortcutManagerV3::ShortcutManagerV3(QObject *parent, ShortcutController *controller,
                                       SeatProvider seats)
    : QObject(parent)
    , d(std::make_unique<ShortcutManagerV3Private>(std::move(seats)))
{
    d->m_controller = controller ? controller : new ShortcutController(this);
}

ShortcutManagerV3::~ShortcutManagerV3() = default;

void ShortcutManagerV3::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void ShortcutManagerV3::destroy(WServer *server)
{
    Q_UNUSED(server);
    d->globalRemove();
    Q_EMIT before_destroy();
}

wl_global *ShortcutManagerV3::global() const
{
    return d->global();
}

bool ShortcutManagerV3::tryHandleCaptureEvent(WSeat *seat, QInputEvent *event)
{
    return d->tryHandleCaptureEvent(seat, event);
}

bool ShortcutManagerV3::isCaptureActive() const
{
    return d->m_pendingCapture || d->m_drainKey != Qt::Key_unknown;
}

QByteArrayView ShortcutManagerV3::interfaceName() const
{
    return "treeland_shortcut_manager_v3";
}

ShortcutController* ShortcutManagerV3::controller()
{
    return d->m_controller;
}

void ShortcutManagerV3::sendActivated(const QString& name, ShortcutController::KeyFlags keyFlags)
{
    d->sendActivated(name, keyFlags);
}

void ShortcutManagerV3Private::cancelCapture()
{
    if (m_pendingCapture)
        resetCaptureState()->sendFailed(ShortcutCaptureV3::failed_reason_aborted);
    m_drainKey = Qt::Key_unknown;
    m_drainSeat = nullptr;
}

void ShortcutManagerV3::cancelCapture()
{
    d->cancelCapture();
}
