// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <wglobal.h>

#include <qwglobal.h>

#include <QObject>
#include <QVector>
#include <QList>
#include <QPointer>
#include <QHash>

Q_MOC_INCLUDE("workspace/workspace.h")

class Helper;
class SurfaceWrapper;
class RootSurfaceContainer;
class LayerSurfaceContainer;
class Workspace;
class SurfaceContainer;
class PopupSurfaceContainer;
class QmlEngine;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WXdgToplevelSurface;
class WXdgPopupSurface;
class WXdgShell;
class WLayerShell;
class WLayerSurface;
class WXWayland;
class WToplevelSurface; // forward declare base toplevel
class WInputMethodHelper;
class WInputPopupSurface;
class WSeat;
class WSurface;
class WXWaylandSurface;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_compositor;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

class AppIdResolverManager; // forward declare new protocol manager
class WindowSizeStore; // forward declare size store

class ShellHandler : public QObject
{
    friend class Helper;
    Q_OBJECT

public:
    explicit ShellHandler(RootSurfaceContainer *rootContainer);
    [[nodiscard]] Workspace *workspace() const;

    void createComponent(QmlEngine *engine);
    void initXdgShell(WAYLIB_SERVER_NAMESPACE::WServer *server);
    void initLayerShell(WAYLIB_SERVER_NAMESPACE::WServer *server);
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXWayland *createXWayland(
        WAYLIB_SERVER_NAMESPACE::WServer *server,
        WAYLIB_SERVER_NAMESPACE::WSeat *seat,
        QW_NAMESPACE::qw_compositor *compositor,
        bool lazy);
    // FIXME: never call removeXWayland in treeland.cpp
    void removeXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland);
    void initInputMethodHelper(WAYLIB_SERVER_NAMESPACE::WServer *server,
                               WAYLIB_SERVER_NAMESPACE::WSeat *seat);

    WAYLIB_SERVER_NAMESPACE::WXWayland *defaultXWaylandSocket() const;
Q_SIGNALS:
    void surfaceWrapperAdded(SurfaceWrapper *wrapper);
    void surfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);

private Q_SLOTS:
    void onXdgToplevelSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onXdgToplevelSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);

    void onXdgPopupSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *surface);
    void onXdgPopupSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *surface);

    void onXWaylandSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface);

    void onLayerSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);
    void onLayerSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);

    void onInputPopupSurfaceV2Added(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);
    void onInputPopupSurfaceV2Removed(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);

private:
    void setupSurfaceActiveWatcher(SurfaceWrapper *wrapper);
    void setupSurfaceWindowMenu(SurfaceWrapper *wrapper);
    void updateLayerSurfaceContainer(SurfaceWrapper *surface);
    void handleDdeShellSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                                    SurfaceWrapper *wrapper);
    void setResourceManagerAtom(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland,
                                const QByteArray &value);
    // 预启动闪屏相关：在 PrelaunchSplash::splashRequested 时创建的预启动 SurfaceWrapper
    void handlePrelaunchSplashRequested(const QString &appId);

    // --- helpers (internal) ---
    SurfaceWrapper *matchOrCreateXdgWrapper(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                                            const QString &appId);
    void initXdgWrapperCommon(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                              SurfaceWrapper *wrapper);
    SurfaceWrapper *matchOrCreateXwaylandWrapper(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                                 const QString &appId);
    void initXwaylandWrapperCommon(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                   SurfaceWrapper *wrapper);

    WAYLIB_SERVER_NAMESPACE::WXdgShell *m_xdgShell = nullptr;
    WAYLIB_SERVER_NAMESPACE::WLayerShell *m_layerShell = nullptr;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *m_inputMethodHelper = nullptr;
    QList<WAYLIB_SERVER_NAMESPACE::WXWayland *> m_xwaylands;

    QPointer<RootSurfaceContainer> m_rootSurfaceContainer;
    LayerSurfaceContainer *m_backgroundContainer = nullptr;
    LayerSurfaceContainer *m_bottomContainer = nullptr;
    Workspace *m_workspace = nullptr;
    LayerSurfaceContainer *m_topContainer = nullptr;
    LayerSurfaceContainer *m_overlayContainer = nullptr;
    // FIXME: https://github.com/linuxdeepin/treeland/pull/428 Caused damage to the tooltip
    // Need to find a better way to handle popup click events
    SurfaceContainer *m_popupContainer = nullptr;
    QObject *m_windowMenu = nullptr;
    // 保存预启动(尚未绑定真实 shellSurface) 的 wrapper 列表
    QVector<SurfaceWrapper *> m_prelaunchWrappers;
    // Pending toplevel surfaces (XDG or XWayland) awaiting asynchronous AppId resolve.
    // A callback only proceeds if the surface pointer is still present in this list.
    QVector<WAYLIB_SERVER_NAMESPACE::WToplevelSurface *> m_pendingAppIdResolveToplevels;
    // New protocol based app id resolver (optional, may be null if module not loaded)
    AppIdResolverManager *m_appIdResolverManager = nullptr;
    WindowSizeStore *m_windowSizeStore = nullptr; // 持久化窗口尺寸
};

