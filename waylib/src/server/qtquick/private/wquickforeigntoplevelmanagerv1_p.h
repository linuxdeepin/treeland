// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")

struct wlr_foreign_toplevel_handle_v1_maximized_event;
struct wlr_foreign_toplevel_handle_v1_minimized_event;
struct wlr_foreign_toplevel_handle_v1_activated_event;
struct wlr_foreign_toplevel_handle_v1_fullscreen_event;
struct wlr_foreign_toplevel_handle_v1_set_rectangle_event;

QW_BEGIN_NAMESPACE
class QWForeignToplevelManagerV1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WQuickForeignToplevelManagerV1;
class WQuickForeignToplevelManagerAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    WQuickForeignToplevelManagerAttached(WSurface *target, WQuickForeignToplevelManagerV1 *manager);

Q_SIGNALS:
    void requestMaximize(bool maximized);
    void requestMinimize(bool minimized);
    void requestActivate(bool activated);
    void requestFullscreen(bool fullscreen);
    void requestClose();
    void rectangleChanged(const QRect &rect);

private:
    WSurface *m_target;
    WQuickForeignToplevelManagerV1 *m_manager;
};

class WXdgSurface;
class WOutput;
class WQuickForeignToplevelManagerV1Private;
class WQuickForeignToplevelManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ForeignToplevelManagerV1)
    W_DECLARE_PRIVATE(WQuickForeignToplevelManagerV1)
    QML_ATTACHED(WQuickForeignToplevelManagerAttached)

public:
    explicit WQuickForeignToplevelManagerV1(QObject *parent = nullptr);

    Q_INVOKABLE void add(WXdgSurface *surface);
    Q_INVOKABLE void remove(WXdgSurface *surface);

    static WQuickForeignToplevelManagerAttached *qmlAttachedProperties(QObject *target);

Q_SIGNALS:
    void requestMaximize(WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_maximized_event *event);
    void requestMinimize(WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_minimized_event *event);
    void requestActivate(WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_activated_event *event);
    void requestFullscreen(WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_fullscreen_event *event);
    void requestClose(WXdgSurface *surface);
    void rectangleChanged(WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_set_rectangle_event *event);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPAQUE_POINTER(wlr_foreign_toplevel_handle_v1_maximized_event*);
Q_DECLARE_OPAQUE_POINTER(wlr_foreign_toplevel_handle_v1_minimized_event*);
Q_DECLARE_OPAQUE_POINTER(wlr_foreign_toplevel_handle_v1_activated_event*);
Q_DECLARE_OPAQUE_POINTER(wlr_foreign_toplevel_handle_v1_fullscreen_event*);
Q_DECLARE_OPAQUE_POINTER(wlr_foreign_toplevel_handle_v1_set_rectangle_event*);
