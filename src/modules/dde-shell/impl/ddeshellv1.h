// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "dde-shell-protocol.h"

#include <wayland-server-core.h>
#include <wsurface.h>

#include <qwdisplay.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE

class treeland_dde_shell_manager_v1;

class treeland_window_overlap_checker : public QObject
{
    Q_OBJECT
public:
    explicit treeland_window_overlap_checker(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t id);
    ~treeland_window_overlap_checker();

    enum Anchor {
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

    void sendOverlapped(bool overlapped);

Q_SIGNALS:
    void before_destroy();
    void refresh();

private:
    friend class treeland_dde_shell_manager_v1;
    struct wl_resource *m_resource;

private:
    treeland_dde_shell_manager_v1 *m_manager{ nullptr };
    bool m_alreadySend{ false };
    bool m_overlapped{ false };
};

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

Q_SIGNALS:
    void before_destroy();
    void windowOverlapCheckerCreated(treeland_window_overlap_checker *handle);

private:
    friend class treeland_window_overlap_checker;
    void addWindowOverlapChecker(treeland_window_overlap_checker *handle);

private:
    QList<treeland_window_overlap_checker *> m_checkHandles;
};
