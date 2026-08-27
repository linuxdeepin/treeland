// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>

#include <memory>

struct wlr_keyboard_shortcuts_inhibitor_v1;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_surface;
struct wlr_seat;

WAYLIB_SERVER_USE_NAMESPACE

class KeyboardShortcutsInhibitManagerV1Private;

class KeyboardShortcutsInhibitManagerV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit KeyboardShortcutsInhibitManagerV1(QObject *parent = nullptr);
    ~KeyboardShortcutsInhibitManagerV1() override;

    QByteArrayView interfaceName() const override;

    // Returns true if keyboard shortcuts are currently inhibited for the
    // given surface+seat pair (i.e. an active inhibitor exists).
    bool isInhibited(wlr_seat *seat, wlr_surface *surface) const;

    // Force-deactivate the active inhibitor for the given seat, sending the
    // 'inactive' event to the client. Used by the compositor's built-in
    // non-inhibitable escape-hatch shortcuts (e.g. Ctrl+Alt+Fn VT switch).
    void deactivateActiveInhibitor(wlr_seat *seat);

protected: // WServerInterface
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<KeyboardShortcutsInhibitManagerV1Private> d;
};
