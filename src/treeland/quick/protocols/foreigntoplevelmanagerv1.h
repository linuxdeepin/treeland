// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "foreign-toplevel-manager-server-protocol.h"
#include "foreign_toplevel_manager_impl.h"

#include <wxdgsurface.h>
#include <wquickwaylandserver.h>

#include <QObject>
#include <QQmlEngine>

struct treeland_foreign_toplevel_handle_v1_maximized_event;
struct treeland_foreign_toplevel_handle_v1_minimized_event;
struct treeland_foreign_toplevel_handle_v1_activated_event;
struct treeland_foreign_toplevel_handle_v1_fullscreen_event;
struct treeland_foreign_toplevel_handle_v1_set_rectangle_event;

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class QWForeignToplevelManagerV1Private;

class QW_EXPORT QWForeignToplevelManagerV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QWForeignToplevelManagerV1)
public:
    inline ztreeland_foreign_toplevel_manager_v1 *handle() const
    {
        return QWObject::handle<ztreeland_foreign_toplevel_manager_v1>();
    }

    static QWForeignToplevelManagerV1 *get(ztreeland_foreign_toplevel_manager_v1 *handle);
    static QWForeignToplevelManagerV1 *from(ztreeland_foreign_toplevel_manager_v1 *handle);
    static QWForeignToplevelManagerV1 *create(QWDisplay *display);

    void topLevel(Waylib::Server::WToplevelSurface *surface);
    void close(Waylib::Server::WToplevelSurface *surface);
    void done(Waylib::Server::WToplevelSurface *surface);
    void setPid(Waylib::Server::WToplevelSurface *surface, uint32_t pid);
    void setIdentifier(Waylib::Server::WToplevelSurface *surface, const QString &identifier);
    void updateSurfaceInfo(Waylib::Server::WToplevelSurface *surface);

Q_SIGNALS:
    void beforeDestroy(QWForeignToplevelManagerV1 *self);

private:
    QWForeignToplevelManagerV1(ztreeland_foreign_toplevel_manager_v1 *handle, bool isOwner);
    ~QWForeignToplevelManagerV1() = default;
};

WAYLIB_SERVER_BEGIN_NAMESPACE
class WXdgSurface;
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

class QuickForeignToplevelManagerV1;
class QuickForeignToplevelManagerAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    QuickForeignToplevelManagerAttached(WSurface *target, QuickForeignToplevelManagerV1 *manager);

Q_SIGNALS:
    void requestMaximize(bool maximized);
    void requestMinimize(bool minimized);
    void requestActivate(bool activated);
    void requestFullscreen(bool fullscreen);
    void requestClose();
    void rectangleChanged(const QRect &rect);

private:
    WSurface *m_target;
    QuickForeignToplevelManagerV1 *m_manager;
};

class QuickForeignToplevelManagerV1Private;
class QuickForeignToplevelManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TreeLandForeignToplevelManagerV1)
    W_DECLARE_PRIVATE(QuickForeignToplevelManagerV1)
    QML_ATTACHED(QuickForeignToplevelManagerAttached)

public:
    explicit QuickForeignToplevelManagerV1(QObject *parent = nullptr);

    Q_INVOKABLE void add(WToplevelSurface *surface);
    Q_INVOKABLE void remove(WToplevelSurface *surface);

    Q_INVOKABLE void enterDockPreview(WSurface *relative_surface);
    Q_INVOKABLE void leaveDockPreview(WSurface *relative_surface);

    static QuickForeignToplevelManagerAttached *qmlAttachedProperties(QObject *target);

Q_SIGNALS:
    void requestMaximize(WToplevelSurface *surface, treeland_foreign_toplevel_handle_v1_maximized_event *event);
    void requestMinimize(WToplevelSurface *surface, treeland_foreign_toplevel_handle_v1_minimized_event *event);
    void requestActivate(WToplevelSurface *surface, treeland_foreign_toplevel_handle_v1_activated_event *event);
    void requestFullscreen(WToplevelSurface *surface, treeland_foreign_toplevel_handle_v1_fullscreen_event *event);
    void requestClose(WToplevelSurface *surface);
    void rectangleChanged(WToplevelSurface *surface, treeland_foreign_toplevel_handle_v1_set_rectangle_event *event);
    void requestDockPreview(std::vector<WSurface*> surfaces, WSurface *target, QPoint abs, int direction);
    void requestDockClose();

private:
    void create() override;
};

Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_maximized_event*);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_minimized_event*);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_activated_event*);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_fullscreen_event*);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_set_rectangle_event*);
