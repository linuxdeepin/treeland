// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wsurface.h>
#include <wserver.h>

#include <QPoint>
#include <QObject>
#include <optional>

WAYLIB_SERVER_USE_NAMESPACE

class DDEShellManagerInterfaceV2Private;
class DDEShellSurfaceV2;

// dde/treeland-dde-shell-unstable-v2.xml (treeland-protocols 0.7.0).
// The v2 manager only creates shell surfaces; the auxiliary resources of v1
// (active, multitaskview, picker, lockscreen, overlap checker) live in their
// own split protocols now.
class DDEShellManagerInterfaceV2 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit DDEShellManagerInterfaceV2(QObject *parent = nullptr);
    ~DDEShellManagerInterfaceV2() override;

    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 1;
Q_SIGNALS:
    void surfaceCreated(DDEShellSurfaceV2 *interface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<DDEShellManagerInterfaceV2Private> d;
};

class DDEShellSurfaceV2Private;
class DDEShellSurfaceV2 : public QObject
{
    Q_OBJECT
public:
    // treeland_dde_shell_surface_v2.role, 0-based since v2
    enum Role {
        OVERLAY = 0,
    };

    // treeland_dde_shell_surface_v2.skip_flag bitfield
    enum SkipFlag {
        SkipSwitcher = 0x1,
        SkipDockPreview = 0x2,
        SkipMultitaskView = 0x4,
    };

    ~DDEShellSurfaceV2() override;

    WSurface *wSurface() const;
    DDEShellSurfaceV2::Role role() const;
    // Global-space position hint, resolved at request time from the
    // output-relative coordinates of set_position_hint. Has a value iff the
    // most recent placement hint was set_position_hint.
    std::optional<QPoint> positionHint() const;
    // (x_offset, y_offset) from the cursor, as sent by
    // set_cursor_placement_hint. Has a value iff the most recent placement
    // hint was set_cursor_placement_hint.
    std::optional<QPoint> cursorPlacementHint() const;
    uint32_t skipFlags() const;
    bool acceptKeyboardFocus() const;

    static DDEShellSurfaceV2 *get(wl_resource *native);
    static DDEShellSurfaceV2 *get(WSurface *surface);

Q_SIGNALS:
    void roleChanged(DDEShellSurfaceV2::Role role);
    void positionHintChanged(QPoint pos);
    void cursorPlacementHintChanged(QPoint offset);
    void skipFlagsChanged(quint32 flags);
    void acceptKeyboardFocusChanged(bool accept);

private:
    explicit DDEShellSurfaceV2(wl_resource *surface, wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV2Private;
    friend class DDEShellSurfaceV2Private;
    std::unique_ptr<DDEShellSurfaceV2Private> d;
};
