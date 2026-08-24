// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "keyboardshortcutsinhibitmanager.h"
#include "seat/helper.h"
#include "seatsmanager.h"
#include "common/treelandlogging.h"

#include <wsurface.h>
#include <wscoplistener.h>

extern "C" {
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
}

#include <QHash>

#include <algorithm>

struct InhibitorEntry {
    wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = nullptr;
    WScopedListener destroyListener;
};

class KeyboardShortcutsInhibitManagerV1Private
{
public:
    explicit KeyboardShortcutsInhibitManagerV1Private(KeyboardShortcutsInhibitManagerV1 *q)
        : q(q) {}

    KeyboardShortcutsInhibitManagerV1 *q = nullptr;
    WScopedListener newInhibitorListener;
    std::vector<InhibitorEntry> inhibitors;
    QHash<WSeat *, QMetaObject::Connection> seatFocusConnections;
    bool seatConnectionsSetup = false;

    wlr_keyboard_shortcuts_inhibit_manager_v1 *handle() const
    {
        return reinterpret_cast<wlr_keyboard_shortcuts_inhibit_manager_v1 *>(q->handle());
    }

    void setupSeatConnections();
    void onNewInhibitor(wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor);
    void onInhibitorDestroy(wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor);
    void onSeatFocusChanged(WSeat *seat);
    void handleSeatAdded(WSeat *seat);
    void handleSeatRemoved(WSeat *seat);
};

void KeyboardShortcutsInhibitManagerV1Private::setupSeatConnections()
{
    if (seatConnectionsSetup)
        return;
    seatConnectionsSetup = true;

    auto *helper = Helper::instance();
    auto *seatManager = helper->seatManager();
    const auto seats = seatManager->seats();
    for (auto *seat : seats) {
        handleSeatAdded(seat);
    }

    QObject::connect(seatManager, &SeatsManager::seatAdded, q, [this](WSeat *seat) {
        handleSeatAdded(seat);
    });
    QObject::connect(seatManager, &SeatsManager::seatRemoved, q, [this](WSeat *seat) {
        handleSeatRemoved(seat);
    });
}

void KeyboardShortcutsInhibitManagerV1Private::handleSeatAdded(WSeat *seat)
{
    if (seatFocusConnections.contains(seat))
        return;

    auto conn = QObject::connect(seat, &WSeat::keyboardFocusSurfaceChanged, q, [this, seat]() {
        onSeatFocusChanged(seat);
    });
    seatFocusConnections.insert(seat, conn);

    // Activate inhibitors for surfaces that already have focus on this seat.
    onSeatFocusChanged(seat);
}

void KeyboardShortcutsInhibitManagerV1Private::handleSeatRemoved(WSeat *seat)
{
    auto it = seatFocusConnections.find(seat);
    if (it != seatFocusConnections.end()) {
        QObject::disconnect(it.value());
        seatFocusConnections.erase(it);
    }
}

void KeyboardShortcutsInhibitManagerV1Private::onNewInhibitor(wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
    qCInfo(lcTlKeyboardShortcutsInhibit) << "New inhibitor created for surface"
                                          << inhibitor->surface << "seat" << inhibitor->seat;

    InhibitorEntry entry;
    entry.inhibitor = inhibitor;
    entry.destroyListener.init(&inhibitor->events.destroy, q, [this, inhibitor](void *) {
        onInhibitorDestroy(inhibitor);
    });
    inhibitors.push_back(std::move(entry));

    // If the surface already has keyboard focus on this seat, activate the
    // inhibitor immediately (sends 'active' to the client).
    auto *wSeat = WSeat::fromHandle(inhibitor->seat);
    if (wSeat) {
        auto *focusSurface = wSeat->keyboardFocusSurface();
        if (focusSurface && focusSurface->handle() == inhibitor->surface) {
            wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);
        }
    }
}

void KeyboardShortcutsInhibitManagerV1Private::onInhibitorDestroy(wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
    qCInfo(lcTlKeyboardShortcutsInhibit) << "Inhibitor destroyed for surface"
                                         << inhibitor->surface << "seat" << inhibitor->seat;

    inhibitors.erase(std::remove_if(inhibitors.begin(), inhibitors.end(),
                                    [inhibitor](const InhibitorEntry &e) {
                                        return e.inhibitor == inhibitor;
                                    }),
                     inhibitors.end());
}

void KeyboardShortcutsInhibitManagerV1Private::onSeatFocusChanged(WSeat *seat)
{
    wlr_seat *wlrSeat = seat->handle();
    auto *focusSurface = seat->keyboardFocusSurface();
    wlr_surface *wlrSurface = focusSurface ? focusSurface->handle() : nullptr;

    for (auto &entry : inhibitors) {
        if (entry.inhibitor->seat != wlrSeat)
            continue;

        if (wlrSurface && entry.inhibitor->surface == wlrSurface) {
            // Keyboard focus entered this surface: activate the inhibitor.
            // wlr_keyboard_shortcuts_inhibitor_v1_activate() is idempotent
            // (only sends 'active' when not already active), so this safely
            // re-activates on focus regain.
            wlr_keyboard_shortcuts_inhibitor_v1_activate(entry.inhibitor);
        } else if (entry.inhibitor->active) {
            // Keyboard focus left this surface: restore the compositor's own
            // shortcuts. Per the protocol, no 'inactive' event is sent on
            // focus leave, so reset 'active' directly (bypassing deactivate())
            // to let activate() re-fire on the next focus enter.
            entry.inhibitor->active = false;
        }
    }
}

KeyboardShortcutsInhibitManagerV1::KeyboardShortcutsInhibitManagerV1(QObject *parent)
    : QObject(parent)
    , d(new KeyboardShortcutsInhibitManagerV1Private(this))
{
}

KeyboardShortcutsInhibitManagerV1::~KeyboardShortcutsInhibitManagerV1() = default;

QByteArrayView KeyboardShortcutsInhibitManagerV1::interfaceName() const
{
    return "zwp_keyboard_shortcuts_inhibit_manager_v1";
}

void KeyboardShortcutsInhibitManagerV1::create(WServer *server)
{
    m_handle = wlr_keyboard_shortcuts_inhibit_v1_create(server->handle());
    if (!m_handle) {
        qCWarning(lcTlKeyboardShortcutsInhibit) << "Failed to create keyboard shortcuts inhibit manager";
        return;
    }

    d->newInhibitorListener.init(&d->handle()->events.new_inhibitor, this, [this](void *data) {
        auto *inhibitor = static_cast<wlr_keyboard_shortcuts_inhibitor_v1 *>(data);
        d->onNewInhibitor(inhibitor);
    });

    d->setupSeatConnections();
}

void KeyboardShortcutsInhibitManagerV1::destroy([[maybe_unused]] WServer *server)
{
    d->newInhibitorListener.disconnect();
    for (auto &entry : d->inhibitors)
        entry.destroyListener.disconnect();
    d->inhibitors.clear();

    for (auto &conn : d->seatFocusConnections)
        QObject::disconnect(conn);
    d->seatFocusConnections.clear();
    d->seatConnectionsSetup = false;

    // The wlr manager is reclaimed by display.reset() in WServer::stop();
    // null m_handle so handle()/global() return null instead of a dangling
    // pointer.
    m_handle = nullptr;
}

wl_global *KeyboardShortcutsInhibitManagerV1::global() const
{
    return d->handle() ? d->handle()->global : nullptr;
}

bool KeyboardShortcutsInhibitManagerV1::isInhibited(wlr_seat *seat, wlr_surface *surface) const
{
    for (const auto &entry : d->inhibitors) {
        if (entry.inhibitor->active && entry.inhibitor->seat == seat && entry.inhibitor->surface == surface)
            return true;
    }
    return false;
}

void KeyboardShortcutsInhibitManagerV1::deactivateActiveInhibitor(wlr_seat *seat)
{
    for (auto &entry : d->inhibitors) {
        if (entry.inhibitor->seat == seat && entry.inhibitor->active)
            wlr_keyboard_shortcuts_inhibitor_v1_deactivate(entry.inhibitor);
    }
}
