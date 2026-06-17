// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "keyboardstatenotifymanagerinterfacev1.h"
#include "qwayland-server-treeland-keyboard-state-notify-unstable-v1.h"
#include "seat/helper.h"
#include "seatsmanager.h"
#include "common/treelandlogging.h"

#include <qwdisplay.h>
#include <qwkeyboard.h>
#include <qwinputdevice.h>
#include <qwseat.h>

#include <wbackend.h>
#include <winputdevice.h>

#include <xkbcommon/xkbcommon.h>

static QList<KeyboardStateWatcherV1 *> s_watchers;
static QHash<WSeat *, uint32_t> s_lastLockStates;

struct ModifierInfo {
    uint32_t flag;
    const char *ledName;
};

static constexpr ModifierInfo s_modifierInfos[] = {
    { KeyboardStateWatcherV1::CapsLock, XKB_LED_NAME_CAPS },
    { KeyboardStateWatcherV1::NumLock, XKB_LED_NAME_NUM },
};

static uint32_t getLockStateBitfield(xkb_state *state)
{
    uint32_t locked = 0;
    auto *keymap = xkb_state_get_keymap(state);

    for (const auto &info : s_modifierInfos) {
        xkb_led_index_t idx = xkb_keymap_led_get_index(keymap, info.ledName);
        if (idx != XKB_LED_INVALID
            && xkb_state_led_index_is_active(state, idx))
            locked |= info.flag;
    }

    return locked;
}

struct KeyboardConnection {
    QMetaObject::Connection modifiersConnection;

    QPointer<WSeat> seat = nullptr;
};

class TreelandKeyboardStateNotifyManagerInterfaceV1Private
    : public QtWaylandServer::treeland_keyboard_state_notify_manager_v1
{
public:
    explicit TreelandKeyboardStateNotifyManagerInterfaceV1Private(TreelandKeyboardStateNotifyManagerInterfaceV1 *_q);

    TreelandKeyboardStateNotifyManagerInterfaceV1 *q = nullptr;

    void setupKeyboardConnections();
    void onModifiersEvent(WSeat *seat);
    wl_global *global() const;

protected:
    void bind_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void get_keyboard_state_watcher(Resource *resource,
                                     uint32_t id,
                                     struct ::wl_resource *seat) override;

private:
    void handleSeatAdded(WSeat *seat);
    void handleSeatDestroy(WSeat *seat);
    void connectKeyboardGroup(WSeat *seat, WInputDevice *keyboardDevice);

    QList<KeyboardConnection> m_keyboardConnections;
};

TreelandKeyboardStateNotifyManagerInterfaceV1Private::TreelandKeyboardStateNotifyManagerInterfaceV1Private(
    TreelandKeyboardStateNotifyManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *TreelandKeyboardStateNotifyManagerInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::bind_resource([[maybe_unused]] Resource *resource)
{
    if (resourceMap().isEmpty()) {
        setupKeyboardConnections();
    }
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::connectKeyboardGroup(WSeat *seat, WInputDevice *keyboardDevice)
{
    for (const auto &conn : std::as_const(m_keyboardConnections)) {
        if (conn.seat == seat)
            return;
    }

    KeyboardConnection conn;
    conn.seat = seat;
    conn.modifiersConnection = keyboardDevice->safeConnect(&qw_keyboard::notify_modifiers, q, [this, seat] () {
        onModifiersEvent(seat);
    });
    m_keyboardConnections.push_back(conn);
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::setupKeyboardConnections()
{
    auto *helper = Helper::instance();
    const auto seats = helper->seatManager()->seats();
    for (auto *seat : seats) {
        handleSeatAdded(seat);
    }

    QObject::connect(helper->seatManager(), &SeatsManager::seatAdded, q, [this](WSeat *seat) {
        handleSeatAdded(seat);
    });
    QObject::connect(helper->seatManager(), &SeatsManager::seatRemoved, q, [this](WSeat *seat) {
        handleSeatDestroy(seat);
    });
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::onModifiersEvent(WSeat *seat)
{
    if (s_watchers.isEmpty())
        return;

    qw_keyboard *keyboard = qobject_cast<qw_keyboard*>(seat->keyboardGroupKeyboard()->handle());
    const auto *wlrKeyboard = keyboard->handle();
    if (!wlrKeyboard || !wlrKeyboard->xkb_state)
        return;

    uint32_t currentLocks = getLockStateBitfield(wlrKeyboard->xkb_state);
    uint32_t prevLocks = s_lastLockStates.value(seat, 0);
    uint32_t changed = currentLocks ^ prevLocks;

    if (changed == 0)
        return;

    s_lastLockStates[seat] = currentLocks;

    for (auto *watcher : std::as_const(s_watchers)) {
        if (watcher->seat() && watcher->wSeat() != seat)
            continue;

        uint32_t watchMods = watcher->modifiers() & changed;
        if (watchMods == 0)
            continue;

        for (int i = 0; i < 2; ++i) {
            uint32_t mod = (1u << i);
            if (!(watchMods & mod))
                continue;

            bool isLocked = (currentLocks & mod) != 0;
            if (isLocked && !(watcher->flags() & KeyboardStateWatcherV1::WatchLocked))
                continue;
            if (!isLocked && !(watcher->flags() & KeyboardStateWatcherV1::WatchUnlocked))
                continue;

            watcher->sendStateChanged(mod, isLocked ? KeyboardStateWatcherV1::Locked : KeyboardStateWatcherV1::Unlocked);
        }
    }
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::handleSeatDestroy(WSeat *seat)
{
    for (int i = m_keyboardConnections.size() - 1; i >= 0; --i) {
        if (m_keyboardConnections[i].seat == seat) {
            QObject::disconnect(m_keyboardConnections[i].modifiersConnection);
            m_keyboardConnections.removeAt(i);
        }
    }
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::destroy(Resource *resource)
{
    if (resourceMap().size() == 1) {
        m_keyboardConnections.clear();
    }

    wl_resource_destroy(resource->handle);
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::get_keyboard_state_watcher(Resource *resource,
                                                                                       uint32_t id,
                                                                                       struct ::wl_resource *seat)
{
    wl_resource *watcherResource = wl_resource_create(resource->client(),
                                                      &treeland_keyboard_state_watcher_v1_interface,
                                                      resource->version(),
                                                      id);
    if (!watcherResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *watcher = new KeyboardStateWatcherV1(watcherResource, seat);
    s_watchers.append(watcher);

    QObject::connect(watcher, &QObject::destroyed, [watcher]() {
        s_watchers.removeOne(watcher);
    });

    Q_EMIT q->keyboardStateWatcherCreated(watcher);
}

void TreelandKeyboardStateNotifyManagerInterfaceV1Private::handleSeatAdded(WSeat *seat)
{
    auto *keyboardevice = seat->keyboardGroupKeyboard();
    if (keyboardevice) {
        connectKeyboardGroup(seat, keyboardevice);
    }
}

class KeyboardStateWatcherV1Private : public QtWaylandServer::treeland_keyboard_state_watcher_v1
{
public:
    explicit KeyboardStateWatcherV1Private(KeyboardStateWatcherV1 *_q,
                                           wl_resource *resource,
                                           wl_resource *_seat);

    KeyboardStateWatcherV1 *q = nullptr;
    wl_resource *seat = nullptr;
    KeyboardStateWatcherV1::ModifierTypes modifiers;
    KeyboardStateWatcherV1::ModifierTypes pendingModifiers;
    KeyboardStateWatcherV1::WatchFlags flags;
    KeyboardStateWatcherV1::WatchFlags pendingFlags;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_modifiers(Resource *resource, uint32_t modifiers) override;
    void set_flags(Resource *resource, uint32_t flags) override;
    void apply(Resource *resource) override;
};

KeyboardStateWatcherV1Private::KeyboardStateWatcherV1Private(KeyboardStateWatcherV1 *_q,
                                                               wl_resource *resource,
                                                               wl_resource *_seat)
    : QtWaylandServer::treeland_keyboard_state_watcher_v1(resource)
    , q(_q)
    , seat(_seat)
{
}

void KeyboardStateWatcherV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void KeyboardStateWatcherV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardStateWatcherV1Private::set_modifiers(Resource *resource, uint32_t modifiers)
{
    if (!treeland_keyboard_state_watcher_v1_modifier_is_valid(modifiers, resource->version())) {
        qCCritical(lcTlKeyboardNotify, "unknown modifiers");
        return;
    }

    pendingModifiers = KeyboardStateWatcherV1::ModifierTypes(modifiers);
}

void KeyboardStateWatcherV1Private::set_flags(Resource *resource, uint32_t flags)
{
    if (!treeland_keyboard_state_watcher_v1_watch_flag_is_valid(flags, resource->version())) {
        qCCritical(lcTlKeyboardNotify, "unknown watch flags");
        return;
    }

    pendingFlags = KeyboardStateWatcherV1::WatchFlags(flags);
}

void KeyboardStateWatcherV1Private::apply([[maybe_unused]] Resource *resource)
{
    if (pendingModifiers == modifiers && pendingFlags == flags)
        return;

    modifiers = pendingModifiers;
    flags = pendingFlags;

    if (modifiers == 0)
        return;

    auto *wSeat = q->wSeat();
    QList<WSeat *> seats;
    if (wSeat) {
        seats.append(wSeat);
    } else {
        auto *helper = Helper::instance();
        if (helper)
            seats = helper->seatManager()->seats();
    }

    for (auto *seat : std::as_const(seats)) {
        qw_keyboard *keyboard =
            qobject_cast<qw_keyboard *>(seat->keyboardGroupKeyboard()->handle());
        if (!keyboard || !keyboard->handle()->xkb_state)
            continue;

        uint32_t currentLocks = getLockStateBitfield(keyboard->handle()->xkb_state);

        for (int i = 0; i < 2; ++i) {
            uint32_t mod = (1u << i);
            if (!(modifiers & mod))
                continue;

            bool isLocked = (currentLocks & mod) != 0;
            if (isLocked && !(flags & KeyboardStateWatcherV1::WatchLocked))
                continue;
            if (!isLocked && !(flags & KeyboardStateWatcherV1::WatchUnlocked))
                continue;

            q->sendCurrentState(mod, isLocked ? KeyboardStateWatcherV1::Locked : KeyboardStateWatcherV1::Unlocked);
        }
    }
}

TreelandKeyboardStateNotifyManagerInterfaceV1::TreelandKeyboardStateNotifyManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandKeyboardStateNotifyManagerInterfaceV1Private(this))
{
}

TreelandKeyboardStateNotifyManagerInterfaceV1::~TreelandKeyboardStateNotifyManagerInterfaceV1() = default;

void TreelandKeyboardStateNotifyManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void TreelandKeyboardStateNotifyManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *TreelandKeyboardStateNotifyManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandKeyboardStateNotifyManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

KeyboardStateWatcherV1::KeyboardStateWatcherV1(wl_resource *resource,
                                               wl_resource *seat,
                                               QObject *parent)
    : QObject(parent)
    , d(new KeyboardStateWatcherV1Private(this, resource, seat))
{
}

KeyboardStateWatcherV1::~KeyboardStateWatcherV1() = default;

KeyboardStateWatcherV1::ModifierTypes KeyboardStateWatcherV1::modifiers() const
{
    return d->modifiers;
}

KeyboardStateWatcherV1::WatchFlags KeyboardStateWatcherV1::flags() const
{
    return d->flags;
}

wl_resource *KeyboardStateWatcherV1::seat() const
{
    return d->seat;
}

WSeat *KeyboardStateWatcherV1::wSeat() const
{
    if (!d->seat)
        return nullptr;

    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(d->seat);
    Q_ASSERT_X(seat_client, __func__, "KeyboardStateWatcherV1 get wlr_seat_client failed.");
    return WSeat::fromHandle(qw_seat::from(seat_client->seat));
}

void KeyboardStateWatcherV1::sendStateChanged(uint32_t modifier, ModifierState state)
{
    d->send_state_changed(modifier, state);
}

void KeyboardStateWatcherV1::sendCurrentState(uint32_t modifier, ModifierState state)
{
    d->send_current_state(modifier, state);
}
