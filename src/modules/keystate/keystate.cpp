// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "keystate.h"

#include "qwayland-server-keystate.h"

#include "seat/helper.h"

#include <qwdisplay.h>
#include <qwkeyboard.h>

#include <xkbcommon/xkbcommon.h>

// KDE keystate protocol
// reference: https://github.com/KDE/kwin/blob/master/src/wayland/keystate.cpp

#define ORG_KDE_KWIN_KEYSTATE_VERSION 5

class KeyStateV5Private : public QtWaylandServer::org_kde_kwin_keystate
{
public:
    explicit KeyStateV5Private(WSeat *seat, KeyStateV5 *_q);
    wl_global *global() const;
    void setKeyboard(WInputDevice *keyboard);

    KeyStateV5 *q;

    QPointer<WSeat> m_seat;
    QPointer<qw_keyboard> m_keyboard = nullptr;
    QMetaObject::Connection m_keyboardConnection{};

protected:
    void org_kde_kwin_keystate_destroy(Resource *resource) override;
    void org_kde_kwin_keystate_fetchStates(Resource *resource) override;
};

wl_global *KeyStateV5Private::global() const
{
    return m_global;
}

void KeyStateV5Private::setKeyboard(WInputDevice *keyboard)
{
    QObject::disconnect(m_keyboardConnection);
    if (!keyboard || keyboard->type() != WInputDevice::Type::Keyboard) {
        return;
    }
    m_keyboard = qobject_cast<qw_keyboard *>(keyboard->handle());
    Q_ASSERT(m_keyboard);

    m_keyboardConnection = QObject::connect(m_keyboard, &qw_keyboard::notify_modifiers,
                     q, [this]() {
        const auto &resources = resourceMap();
        for (const auto &resource : resources) {
            org_kde_kwin_keystate_fetchStates(resource);
        }
    });
    const auto &resources = resourceMap();
    for (const auto &resource : resources) {
        org_kde_kwin_keystate_fetchStates(resource);
    }
}

void KeyStateV5Private::org_kde_kwin_keystate_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// mirroring KDE behavior
void KeyStateV5Private::org_kde_kwin_keystate_fetchStates(Resource *resource)
{
    if (!m_keyboard) {
        return;
    }
    auto *keyboard = m_keyboard->handle();
    if (!keyboard || !keyboard->keymap || !keyboard->xkb_state) {
        return;
    }

    // Use xkb_state_led_index_is_active instead of keyboard->leds because
    // the notify_modifiers signal is emitted BEFORE keyboard->leds is updated
    auto sendLockKey = [this, resource, keyboard](const char *ledName, KeyStateV5Private::key key) {
        auto idx = xkb_keymap_led_get_index(keyboard->keymap, ledName);
        if (idx == XKB_LED_INVALID)
            return;
        bool active = xkb_state_led_index_is_active(keyboard->xkb_state, idx);
        send_stateChanged(resource->handle, key,
                          active ? KeyStateV5Private::state_locked
                                 : KeyStateV5Private::state_unlocked);
    };

    sendLockKey(XKB_LED_NAME_SCROLL, KeyStateV5Private::key_scrolllock);

    auto sendModifier = [this, resource, keyboard](const char *name, KeyStateV5Private::key key) {
        auto idx = xkb_keymap_mod_get_index(keyboard->keymap, name);
        if (idx == XKB_MOD_INVALID)
            return;
        if (xkb_state_mod_index_is_active(keyboard->xkb_state, idx, XKB_STATE_MODS_LOCKED)) {
            send_stateChanged(resource->handle, key, KeyStateV5Private::state_locked);
        } else if (xkb_state_mod_index_is_active(keyboard->xkb_state, idx, XKB_STATE_MODS_LATCHED)) {
            send_stateChanged(resource->handle, key, KeyStateV5Private::state_latched);
        } else if (xkb_state_mod_index_is_active(keyboard->xkb_state, idx, XKB_STATE_MODS_DEPRESSED)) {
            send_stateChanged(resource->handle, key, KeyStateV5Private::state_pressed);
        } else {
            send_stateChanged(resource->handle, key, KeyStateV5Private::state_unlocked);
        }
    };

    static constexpr int modifierSinceVersion = ORG_KDE_KWIN_KEYSTATE_KEY_ALT_SINCE_VERSION;
    if (resource->version() >= modifierSinceVersion) {
        // for some reason, XKB_MOD_NAME_{MOD1, MOD4, MOD5} are not defined in xkbcommon
        sendModifier(XKB_MOD_NAME_ALT, KeyStateV5Private::key_alt);     // MOD1 in KDE
        sendModifier(XKB_MOD_NAME_SHIFT, KeyStateV5Private::key_shift);
        sendModifier(XKB_MOD_NAME_CTRL, KeyStateV5Private::key_control);
        sendModifier(XKB_MOD_NAME_LOGO, KeyStateV5Private::key_meta);   // MOD4 in KDE
        sendModifier("Mod5", KeyStateV5Private::key_altgr);
    }

    sendLockKey(XKB_LED_NAME_CAPS, KeyStateV5Private::key_capslock);
    sendLockKey(XKB_LED_NAME_NUM, KeyStateV5Private::key_numlock);
}

KeyStateV5Private::KeyStateV5Private(WSeat *seat, KeyStateV5 *_q)
    : q(_q)
    , m_seat(seat)
{
    assert(seat);
    QObject::connect(seat, &WSeat::keyboardChanged,
                     q, [this, seat]() {
        setKeyboard(seat->keyboard());
    });
    setKeyboard(seat->keyboard());
}

KeyStateV5::KeyStateV5(WSeat *seat, QObject *parent)
    : QObject(parent)
    , d(new KeyStateV5Private(seat, this))
{
}

KeyStateV5::~KeyStateV5()
{
}

QByteArrayView KeyStateV5::interfaceName() const
{
    return d->interfaceName();
}

void KeyStateV5::create(WServer *server)
{
    d->init(server->handle()->handle(), ORG_KDE_KWIN_KEYSTATE_VERSION);
}

void KeyStateV5::destroy([[maybe_unused]] WServer *server)
{
}

wl_global *KeyStateV5::global() const
{
    return d->global();
}
