// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

#include <QObject>

struct treeland_foreign_toplevel_handle_v1;
struct treeland_foreign_toplevel_handle_v1_maximized_event;
struct treeland_foreign_toplevel_handle_v1_minimized_event;
struct treeland_foreign_toplevel_handle_v1_activated_event;
struct treeland_foreign_toplevel_handle_v1_fullscreen_event;
struct treeland_foreign_toplevel_handle_v1_set_rectangle_event;
struct treeland_dock_preview_context_v1_preview_event;
struct treeland_foreign_toplevel_manager_v1;

QW_USE_NAMESPACE

QW_BEGIN_NAMESPACE
class QWOutput;
class QWDisplay;
QW_END_NAMESPACE

class TreeLandDockPreviewContextV1;
class TreeLandForeignToplevelManagerV1Private;

class QW_EXPORT TreeLandForeignToplevelManagerV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(TreeLandForeignToplevelManagerV1)
public:
    inline treeland_foreign_toplevel_manager_v1 *handle() const
    {
        return QWObject::handle<treeland_foreign_toplevel_manager_v1>();
    }

    static TreeLandForeignToplevelManagerV1 *get(treeland_foreign_toplevel_manager_v1 *handle);
    static TreeLandForeignToplevelManagerV1 *from(treeland_foreign_toplevel_manager_v1 *handle);
    static TreeLandForeignToplevelManagerV1 *create(QWDisplay *display);

Q_SIGNALS:
    void beforeDestroy(TreeLandForeignToplevelManagerV1 *self);
    void dockPreviewContextCreated(TreeLandDockPreviewContextV1 *context);

private:
    TreeLandForeignToplevelManagerV1(treeland_foreign_toplevel_manager_v1 *handle, bool isOwner);
    ~TreeLandForeignToplevelManagerV1() = default;
};

class TreeLandForeignToplevelHandleV1Private;

class TreeLandForeignToplevelHandleV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(TreeLandForeignToplevelHandleV1)
public:
    ~TreeLandForeignToplevelHandleV1() = default;

    inline treeland_foreign_toplevel_handle_v1 *handle() const
    {
        return QWObject::handle<treeland_foreign_toplevel_handle_v1>();
    }

    static TreeLandForeignToplevelHandleV1 *get(treeland_foreign_toplevel_handle_v1 *handle);
    static TreeLandForeignToplevelHandleV1 *from(treeland_foreign_toplevel_handle_v1 *handle);
    static TreeLandForeignToplevelHandleV1 *create(TreeLandForeignToplevelManagerV1 *manager);

    void outputEnter(QWOutput *output);
    void outputLeave(QWOutput *output);
    void setActivated(bool activated);
    void setAppId(const char *appId);
    void setFullScreen(bool fullScreen);
    void setMaximized(bool maximized);
    void setMinimized(bool minimized);
    void setParent(TreeLandForeignToplevelHandleV1 *parent);
    void setTitle(const char *title);
    void setPid(pid_t pid);
    void setIdentifier(uint32_t identifier);

Q_SIGNALS:
    void beforeDestroy(TreeLandForeignToplevelHandleV1 *self);
    void requestMaximize(treeland_foreign_toplevel_handle_v1_maximized_event *event);
    void requestMinimize(treeland_foreign_toplevel_handle_v1_minimized_event *event);
    void requestActivate(treeland_foreign_toplevel_handle_v1_activated_event *event);
    void requestFullscreen(treeland_foreign_toplevel_handle_v1_fullscreen_event *event);
    void requestClose();
    void rectangleChanged(treeland_foreign_toplevel_handle_v1_set_rectangle_event *event);

private:
    TreeLandForeignToplevelHandleV1(treeland_foreign_toplevel_handle_v1 *handle, bool isOwner);
};

struct treeland_dock_preview_context_v1;
class TreeLandDockPreviewContextV1Private;

class TreeLandDockPreviewContextV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(TreeLandDockPreviewContextV1)
public:
    ~TreeLandDockPreviewContextV1() = default;

    inline treeland_dock_preview_context_v1 *handle() const
    {
        return QWObject::handle<treeland_dock_preview_context_v1>();
    }

    static TreeLandDockPreviewContextV1 *get(treeland_dock_preview_context_v1 *handle);
    static TreeLandDockPreviewContextV1 *from(treeland_dock_preview_context_v1 *handle);

    void enter();
    void leave();

Q_SIGNALS:
    void beforeDestroy(TreeLandDockPreviewContextV1 *self);
    void requestShow(treeland_dock_preview_context_v1_preview_event *event);
    void requestClose();

private:
    TreeLandDockPreviewContextV1(treeland_dock_preview_context_v1 *handle, bool isOwner);
};
