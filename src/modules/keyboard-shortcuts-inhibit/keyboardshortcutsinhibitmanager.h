// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wseat.h>
#include <wserver.h>

#include <QHash>
#include <QObject>

#include <memory>

struct wlr_keyboard_shortcuts_inhibitor_v1;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_surface;
struct wlr_seat;

WAYLIB_SERVER_USE_NAMESPACE

class KeyboardShortcutsInhibitManagerV1Private;

class KeyboardShortcutsInhibitManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsInhibitManagerV1(QObject *parent = nullptr);
    ~KeyboardShortcutsInhibitManagerV1() override;

    QByteArrayView interfaceName() const override;

    bool isInhibited(wlr_seat *seat, wlr_surface *surface) const;
    void deactivateActiveInhibitor(wlr_seat *seat);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void setupSeatConnections();
    void onSeatAdded(WSeat *seat);
    void onSeatRemoved(WSeat *seat);
    void onSeatFocusChanged(WSeat *seat);

    QHash<WSeat *, QMetaObject::Connection> seatFocusConnections;
    bool seatConnectionsSetup = false;

    std::unique_ptr<KeyboardShortcutsInhibitManagerV1Private> d;
};
