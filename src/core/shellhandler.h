// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <wglobal.h>

#include <qwglobal.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>

Q_MOC_INCLUDE("workspace/workspace.h")

QW_BEGIN_NAMESPACE
class qw_buffer;
QW_END_NAMESPACE

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
class WindowConfigStore;    // forward declare config store

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
    // Prelaunch splash related: creates a prelaunch SurfaceWrapper when
    // PrelaunchSplash::splashRequested
    void handlePrelaunchSplashRequested(const QString &appId, QW_NAMESPACE::qw_buffer *iconBuffer);
    void createPrelaunchSplash(const QString &appId,
                               QW_NAMESPACE::qw_buffer *iconBuffer,
                               const QSize &lastSize,
                               const QString &darkPalette,
                               const QString &lightPalette,
                               qlonglong splashThemeType);

    // --- helpers (internal) ---
    SurfaceWrapper *matchOrCreateXdgWrapper(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                                            const QString &appId);
    void initXdgWrapperCommon(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                              SurfaceWrapper *wrapper);
    SurfaceWrapper *matchOrCreateXwaylandWrapper(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                                 const QString &appId);
    void initXwaylandWrapperCommon(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                   SurfaceWrapper *wrapper);
    // Unified parent/container update for Xdg & XWayland toplevel wrappers.
    void updateWrapperContainer(SurfaceWrapper *wrapper,
                                WAYLIB_SERVER_NAMESPACE::WSurface *parentSurface);

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
    // Prelaunch wrappers created before binding to a real shell surface
    QList<SurfaceWrapper *> m_prelaunchWrappers;
    // Prelaunch requests waiting for dconfig initialization
    QSet<QString> m_pendingPrelaunchAppIds;
    // Pending toplevel surfaces (XDG or XWayland) awaiting async AppId resolve; callbacks continue
    // only if the pointer remains in this list
    QList<WAYLIB_SERVER_NAMESPACE::WToplevelSurface *> m_pendingAppIdResolveToplevels;
    // New protocol based app id resolver (optional, may be null if module not loaded)
    AppIdResolverManager *m_appIdResolverManager = nullptr;
    WindowConfigStore *m_windowConfigStore = nullptr;
};
