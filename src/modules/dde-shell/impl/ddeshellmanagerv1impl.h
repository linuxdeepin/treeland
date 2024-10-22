// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "dde-shell-protocol.h"

#include <wayland-server-core.h>
#include <wseat.h>
#include <wsurface.h>

#include <qwdisplay.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE

class treeland_dde_shell_surface : public QObject
{
    Q_OBJECT
public:
    enum Role
    {
        OVERLAY,
    };

    ~treeland_dde_shell_surface();
    bool treeland_dde_shell_surface_is_mapped_to_wsurface(WSurface *surface);
    void destroy();

    treeland_dde_shell_manager_v1 *m_manager{ nullptr };
    wl_resource *m_resource{ nullptr };
    wl_resource *m_surface_resource{ nullptr };

    std::optional<QPoint> m_surfacePos;
    std::optional<Role> m_role;
    // if m_yOffset has_value, preventing surface from being displayed beyond
    // the edge of the output.
    std::optional<uint32_t> m_yOffset;
    std::optional<bool> m_skipSwitcher;
    std::optional<bool> m_skipDockPreView;
    std::optional<bool> m_skipMutiTaskView;

Q_SIGNALS:
    void positionChanged(QPoint pos);
    void roleChanged(treeland_dde_shell_surface::Role role);
    void yOffsetChanged(uint32_t offset);
    void skipSwitcherChanged(bool skip);
    void skipDockPreViewChanged(bool skip);
    void skipMutiTaskViewChanged(bool skip);
    void before_destroy();
};

class treeland_dde_active;

class treeland_dde_shell_manager_v1 : public QObject
{
    Q_OBJECT
    treeland_dde_shell_manager_v1(QW_NAMESPACE::qw_display *display, QObject *parent = nullptr);

public:
    ~treeland_dde_shell_manager_v1();

    wl_event_loop *eventLoop{ nullptr };
    wl_global *global{ nullptr };
    wl_list resources;

    static treeland_dde_shell_manager_v1 *create(QW_NAMESPACE::qw_display *display);
    void addWindowOverlapChecker(treeland_window_overlap_checker *handle);
    void addShellSurface(treeland_dde_shell_surface *handle);
    void addDdeActive(treeland_dde_active *handle);

Q_SIGNALS:
    void before_destroy();
    void windowOverlapCheckerCreated(treeland_window_overlap_checker *handle);
    void shellSurfaceCreated(treeland_dde_shell_surface *handle);
    void ddeActiveCreated(treeland_dde_active *handle);

private:
    QList<treeland_window_overlap_checker *> m_checkHandles;
    QList<treeland_dde_shell_surface *> m_surfaceHandles;
    QList<treeland_dde_active *> m_ddeActiveHandles;

    friend class DDEShellManagerV1;
};

class treeland_window_overlap_checker : public QObject
{
    Q_OBJECT
public:
    ~treeland_window_overlap_checker();

    enum Anchor
    {
        TOP = TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_TOP,
        RIGHT = TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_RIGHT,
        BOTTOM = TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_BOTTOM,
        LEFT = TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_LEFT,
    };
    Q_ENUM(Anchor)

    // FIXME: change to function
    struct wlr_output *m_output;
    QSize m_size;
    Anchor m_anchor;
    treeland_dde_shell_manager_v1 *m_manager{ nullptr };
    wl_resource *m_resource{ nullptr };

    void sendOverlapped(bool overlapped);

Q_SIGNALS:
    void before_destroy();
    void refresh();

private:
    bool m_alreadySend{ false };
    bool m_overlapped{ false };
};

class treeland_dde_active : public QObject
{
    Q_OBJECT
public:
    ~treeland_dde_active();

    void send_active_in(uint32_t reason);
    void send_active_out(uint32_t reason);
    void send_start_drag();
    bool treeland_dde_active_is_mapped_to_wseat(WSeat *seat);

    wl_resource *m_resource{ nullptr };
    wl_resource *m_seat_resource{ nullptr };

Q_SIGNALS:
    void before_destroy();
};
