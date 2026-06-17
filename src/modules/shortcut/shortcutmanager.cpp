// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"

#include "common/treelandlogging.h"
#include "input/gestures.h"
#include "qwayland-server-treeland-shortcut-manager-v2.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "session/session.h"
#include "shortcutcontroller.h"

#include <wseat.h>
#include <wsocket.h>
#include <wsurface.h>

#include <qwdisplay.h>
#include <qwseat.h>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
}

#include <QKeyEvent>
#include <QKeySequence>

#define SHORTCUT_REGISTRATION_SUCCESS 0

using ProtocolAction = QtWaylandServer::treeland_shortcut_manager_v2::action;
static_assert(static_cast<int>(ProtocolAction::action_notify) == static_cast<int>(ShortcutAction::Notify),
              "treeland-shortcut-manager protocol action enum mismatch");
static_assert(static_cast<int>(ProtocolAction::action_taskswitch_sameapp_prev)
              == static_cast<int>(ShortcutAction::TaskSwitchSameAppPrev),
              "treeland-shortcut-manager protocol action enum mismatch");

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

class UserShortcuts {
public:
    QList<KeyShortcut> keys;
    QList<SwipeShortcut> swipes;
    QList<HoldShortcut> holds;

    void append(const UserShortcuts &other)
    {
        keys.append(other.keys);
        swipes.append(other.swipes);
        holds.append(other.holds);
    }
};

// Forward declaration
class ShortcutManagerV2Private;

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

class ShortcutCaptureV1 : public QtWaylandServer::treeland_shortcut_capture_v1
{
public:
    ShortcutCaptureV1(ShortcutManagerV2Private *manager,
                      wl_client *client,
                      uint32_t id,
                      int version)
        : QtWaylandServer::treeland_shortcut_capture_v1(client, id, version)
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
    // Defined after ShortcutManagerV2Private
    void destroy(Resource *resource) override;

    // Called both when the client sends destroy and when the client disconnects.
    // Must clean up m_pendingCapture to avoid a dangling pointer.
    void destroy_resource(Resource *resource) override;

private:
    ShortcutManagerV2Private *m_manager;
    bool m_pending;
};

static SwipeGesture::Direction toSwipeDirection(uint32_t direction)
{
    using Direction = QtWaylandServer::treeland_shortcut_manager_v2::direction;
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

class ShortcutManagerV2Private : public QtWaylandServer::treeland_shortcut_manager_v2
{
public:
    explicit ShortcutManagerV2Private(ShortcutManagerV2 *_q);

    wl_global *global() const;

    uint updateShortcuts(const UserShortcuts& shortcuts, QString &failName);

    void sendActivated(WSocket *socket, const QString &name, ShortcutController::KeyFlags keyFlags);
    void sendCommitSuccess(WSocket *socket);
    void sendCommitFailure(WSocket *socket, const QString &name, uint error);
    void sendInvalidCommit(WSocket *socket);

    ShortcutManagerV2 *q;
    ShortcutController *m_controller = nullptr;

    QMap<WSocket*, Resource*> ownerClients;
    QMap<WSocket*, UserShortcuts> m_shortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingShortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingCommittedShortcuts;
    QMap<WSocket*, QList<QString>> m_pendingDeletes;

    ShortcutCaptureV1 *m_pendingCapture = nullptr;
    // Seat whose keyboard focus was validated at capture start.
    // Events from other seats are ignored during capture.
    WSeat *m_pendingSeat = nullptr;
    Qt::Key m_drainKey = Qt::Key_unknown;
    WSeat *m_drainSeat = nullptr;
    // Target surface that requested the capture. Capture must fail once it loses focus.
    QPointer<WSurface> m_pendingSurface;
    WSocket* m_activeSessionSocket = nullptr;

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
    void commit(Resource *resource) override;
    void unbind(Resource *resource, const QString &name) override;
    void capture_next_shortcut(Resource *resource,
                               struct ::wl_resource *surface,
                               struct ::wl_resource *seat,
                               uint32_t capture) override;

private:
    WSocket *socketFromResource(Resource *resource);
    // Clears all capture-related state and returns the previously pending capture object.
    // The caller is responsible for sending a terminal event on the returned object.
    ShortcutCaptureV1 *resetCaptureState();

public:
    void onCaptureDestroyed(ShortcutCaptureV1 *c);
    bool tryHandleCaptureEvent(WSeat *seat, QInputEvent *event);
};

ShortcutManagerV2Private::ShortcutManagerV2Private(ShortcutManagerV2 *_q)
    : q(_q)
{
}

wl_global *ShortcutManagerV2Private::global() const
{
    return m_global;
}

WSocket *ShortcutManagerV2Private::socketFromResource(Resource *resource)
{
    return WSocket::get(wl_resource_get_client(resource->handle))->rootSocket();
}

uint ShortcutManagerV2Private::updateShortcuts(const UserShortcuts& shortcuts, QString &failName)
{
    uint status = SHORTCUT_REGISTRATION_SUCCESS;
    QList<QString> names;

    const auto tryRegisterAll = [&]() {
        for (const auto& [keybindFlags, name, key, action] : std::as_const(shortcuts.keys)) {
            status = m_controller->registerKey(name, key, keybindFlags, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
        for (const auto& [name, finger, direction, action] : std::as_const(shortcuts.swipes)) {
            status = m_controller->registerSwipeGesture(name, finger, direction, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
        for (const auto& [name, finger, action] : std::as_const(shortcuts.holds)) {
            status = m_controller->registerHoldGesture(name, finger, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
    };

    tryRegisterAll();
    if (status) {
        for (const auto& name : std::as_const(names)) {
            m_controller->unregisterShortcut(name);
        }
    }
    return status;
}

void ShortcutManagerV2Private::sendActivated(WSocket *socket, const QString &name, ShortcutController::KeyFlags keyFlags)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_activated(resource->handle, name, keyFlags.toInt());
}

void ShortcutManagerV2Private::sendCommitSuccess(WSocket *socket)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_commit_success(resource->handle);
}

void ShortcutManagerV2Private::sendCommitFailure(WSocket *socket, const QString &name, uint error)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_commit_failure(resource->handle, name, error);
}

void ShortcutManagerV2Private::sendInvalidCommit(WSocket *socket)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    wl_resource_post_error(resource->handle,
                           error_invalid_commit,
                           "Commit sent before last commit is processed.");
}

void ShortcutManagerV2Private::destroy_resource(Resource *resource)
{
    for (auto it = ownerClients.begin(); it != ownerClients.end(); ) {
        if (it.value() == resource) {
            it = ownerClients.erase(it);
        } else {
            ++it;
        }
    }
}

void ShortcutManagerV2Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShortcutManagerV2Private::acquire(Resource *resource)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.contains(socket)) {
        wl_resource_post_error(resource->handle,
                               error_occupied,
                               "Another client has already acquired the shortcut manager.");
        return;
    }

    // remove stale shortcuts
    m_shortcuts.remove(socket);
    m_pendingShortcuts.remove(socket);
    m_pendingCommittedShortcuts.remove(socket);
    m_pendingDeletes.remove(socket);
    if (m_activeSessionSocket == socket)
        m_controller->clear();
    ownerClients.insert(socket, resource);
}

void ShortcutManagerV2Private::bind_key(Resource *resource,
                                        const QString &name,
                                        const QString &key_sequence,
                                        uint32_t flags,
                                        uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].keys.append(KeyShortcut{
        .keybindFlags = ShortcutController::KeyFlags::fromInt(flags),
        .name = name,
        .key = key_sequence,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::bind_swipe_gesture(Resource *resource,
                                                  const QString &name,
                                                  uint32_t finger,
                                                  uint32_t direction,
                                                  uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].swipes.append(SwipeShortcut{
        .name = name,
        .finger = finger,
        .direction = toSwipeDirection(direction),
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::bind_hold_gesture(Resource *resource,
                                                 const QString &name,
                                                 uint32_t finger,
                                                 uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].holds.append(HoldShortcut{
        .name = name,
        .finger = finger,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::commit(Resource *resource)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    if (!m_pendingShortcuts.contains(socket)) {
        sendCommitSuccess(socket);
        return;
    }

    if (socket != m_activeSessionSocket) {
        if (m_pendingCommittedShortcuts.contains(socket)) {
            sendInvalidCommit(socket);
            return;
        }
        m_pendingCommittedShortcuts[socket] = m_pendingShortcuts.take(socket);
        return;
    }

    const auto pendingShortcuts = m_pendingShortcuts.take(socket);
    QString commitFailName;
    uint status = updateShortcuts(pendingShortcuts, commitFailName);
    if (!status) {
        m_shortcuts[socket].append(pendingShortcuts);
        sendCommitSuccess(socket);
    } else {
        sendCommitFailure(socket, commitFailName, status);
    }
}

void ShortcutManagerV2Private::unbind(Resource *resource, const QString &name)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    if (socket != m_activeSessionSocket) {
        m_pendingDeletes[socket].append(name);
        return;
    }

    m_controller->unregisterShortcut(name);
}

void ShortcutCaptureV1::destroy(Resource *resource)
{
    // destroy_resource() handles cleanup; just trigger it.
    wl_resource_destroy(resource->handle);
}

void ShortcutCaptureV1::destroy_resource(Resource *)
{
    if (m_pending)
        m_manager->onCaptureDestroyed(this);
    delete this;
}

void ShortcutManagerV2Private::capture_next_shortcut(Resource *resource,
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
        new ShortcutCaptureV1(this, resource->client(), capture, resource->version());

    // Check if another capture is already in progress.
    if (m_pendingCapture || m_drainKey != Qt::Key_unknown) {
        captureObj->sendFailed(ShortcutCaptureV1::failed_reason_busy);
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
            requestedSeat = WSeat::fromHandle(qw_seat::from(wlrSeatClient->seat));
    } else if (auto *helper = Helper::instance()) {
        // No seat specified: find whichever seat currently has keyboard focus on this surface.
        const auto &seats = helper->seatManager()->seats();
        for (auto *seat : seats) {
            if (seat->keyboardFocusSurface() == wSurface) {
                requestedSeat = seat;
                break;
            }
        }
    }

    // Validate surface focus / active state.
    auto *focusedSurface = requestedSeat ? requestedSeat->keyboardFocusSurface() : nullptr;
    if (!focusedSurface || focusedSurface != wSurface) {
        captureObj->sendFailed(ShortcutCaptureV1::failed_reason_not_active);
        return;
    }

    m_pendingCapture = captureObj;
    m_pendingSeat = requestedSeat;
    m_pendingSurface = wSurface;
}

void ShortcutManagerV2Private::onCaptureDestroyed(ShortcutCaptureV1 *c)
{
    if (m_pendingCapture == c)
        resetCaptureState();
}

ShortcutCaptureV1 *ShortcutManagerV2Private::resetCaptureState()
{
    auto *c = m_pendingCapture;
    m_pendingCapture = nullptr;
    m_pendingSeat = nullptr;
    m_pendingSurface.clear();
    return c;
}

// Capture event filter — called from Helper::beforeDisposeEvent while a
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
bool ShortcutManagerV2Private::tryHandleCaptureEvent(WSeat *seat, QInputEvent *event)
{
    // Post-capture drain: consume residual KeyRelease events from the captured session.
    // This prevents the release of the captured key from immediately firing a newly
    // bound shortcut that uses KeyRelease trigger semantics.
    if (m_drainKey != Qt::Key_unknown && seat == m_drainSeat
        && event->type() == QEvent::KeyRelease) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (static_cast<Qt::Key>(ke->key()) == m_drainKey) {
            m_drainKey = Qt::Key_unknown;
            m_drainSeat = nullptr;
        }
        return true; // consume all key releases until the captured key is released
    }

    if (!m_pendingCapture)
        return false;

    // Seat filter: ignore events from seats other than the one used to validate focus.
    if (seat != m_pendingSeat)
        return false;

    // If the requesting surface lost focus, abort capture but do not consume the
    // current event, so input can continue to the newly focused target.
    if (!m_pendingSurface || m_pendingSeat->keyboardFocusSurface() != m_pendingSurface) {
        resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_aborted);
        return false;
    }

    // Pointer button or wheel: cancel immediately.
    const auto type = event->type();
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease
        || type == QEvent::Wheel) {
        resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_interrupted);
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
            resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_interrupted);
        }
    } else { // KeyRelease, only needed for modifier-only paths.
        if (isPureModifierKey(key)) {
            // Pure modifier released without any regular key press.
            resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_interrupted);
        } else if (key == Qt::Key_Super_L || key == Qt::Key_Super_R || key == Qt::Key_Meta) {
            // Win/Super is only valid when pressed alone (no other modifiers held).
            if ((kevent->modifiers() & ~Qt::MetaModifier) == Qt::NoModifier)
                resetCaptureState()->sendCaptured(QStringLiteral("Meta"));
            else
                resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_interrupted);
        } else {
            // With press-trigger semantics, regular non-modifier release should not
            // normally reach here. Be defensive and fail-safe if it does.
            resetCaptureState()->sendFailed(ShortcutCaptureV1::failed_reason_interrupted);
        }
    }
    return true;
}

ShortcutManagerV2::ShortcutManagerV2(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ShortcutManagerV2Private>(this))
{
    d->m_controller = new ShortcutController(this);
}

ShortcutManagerV2::~ShortcutManagerV2() = default;

void ShortcutManagerV2::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void ShortcutManagerV2::destroy(WServer *server)
{
    Q_UNUSED(server);
    d->globalRemove();
    Q_EMIT before_destroy();
}

wl_global *ShortcutManagerV2::global() const
{
    return d->global();
}

bool ShortcutManagerV2::tryHandleCaptureEvent(WSeat *seat, QInputEvent *event)
{
    return d->tryHandleCaptureEvent(seat, event);
}

QByteArrayView ShortcutManagerV2::interfaceName() const
{
    return "treeland_shortcut_manager_v2";
}

ShortcutController* ShortcutManagerV2::controller()
{
    return d->m_controller;
}

void ShortcutManagerV2::sendActivated(const QString& name, ShortcutController::KeyFlags keyFlags)
{
    d->sendActivated(d->m_activeSessionSocket, name, keyFlags);
}

void ShortcutManagerV2::onSessionChanged()
{
    QString commitFailName;
    auto session = Helper::instance()->sessionManager()->activeSession().lock();
    if (!session) {
        return;
    }

    auto *socket = session->socket();

    if (d->m_activeSessionSocket == socket)
        return;

    d->m_controller->clear();
    d->m_activeSessionSocket = socket;

    if (d->m_shortcuts.contains(socket)) {
        uint status = d->updateShortcuts(d->m_shortcuts[socket], commitFailName);
        if (status) {
            qCWarning(lcTlShortcut) << "Failed to restore shortcuts" << commitFailName
                                        << "by reason" << status
                                        << "for session" << session->id()
                                        << "for user" << session->username();
        }
        return;
    }

    if (d->m_pendingDeletes.contains(socket)) {
        const auto names = d->m_pendingDeletes.take(socket);
        for (const auto& name : std::as_const(names)) {
            d->m_controller->unregisterShortcut(name);
        }
    }

    if (d->m_pendingCommittedShortcuts.contains(socket)) {
        const auto pendingShortcuts = d->m_pendingCommittedShortcuts.take(socket);
        uint status = d->updateShortcuts(pendingShortcuts, commitFailName);
        if (!status) {
            d->m_shortcuts[socket].append(pendingShortcuts);
            d->sendCommitSuccess(socket);
        } else {
            d->sendCommitFailure(socket, commitFailName, status);
        }
    }
}
