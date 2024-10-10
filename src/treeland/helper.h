// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qmlengine.h"
#include "workspace.h"

#include <wglobal.h>
#include <wqmlcreator.h>
#include <wseat.h>

#include <QObject>
#include <QQmlApplicationEngine>

Q_MOC_INCLUDE(<wtoplevelsurface.h>)
Q_MOC_INCLUDE("surfacewrapper.h")

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WOutputLayout;
class WCursor;
class WBackend;
class WOutputItem;
class WOutputViewport;
class WOutputLayer;
class WOutput;
class WXWayland;
class WInputMethodHelper;
class WXdgDecorationManager;
class WSocket;
class WSurface;
class WToplevelSurface;
class WSurfaceItem;
class WForeignToplevel;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_compositor;
QW_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class Output;
class SurfaceWrapper;
class Workspace;
class RootSurfaceContainer;
class LayerSurfaceContainer;
class ForeignToplevelV1;
class LockScreen;

class Helper : public WSeatEventFilter
{
    friend class RootSurfaceContainer;
    Q_OBJECT
    Q_PROPERTY(bool socketEnabled READ socketEnabled WRITE setSocketEnabled NOTIFY socketEnabledChanged FINAL)
    Q_PROPERTY(SurfaceWrapper* activatedSurface READ activatedSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(RootSurfaceContainer* rootContainer READ rootContainer CONSTANT FINAL)
    Q_PROPERTY(Workspace* workspace READ workspace CONSTANT FINAL)
    Q_PROPERTY(int currentUserId READ currentUserId WRITE setCurrentUserId NOTIFY currentUserIdChanged FINAL)
    Q_PROPERTY(float animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper();

    static Helper *instance();

    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    Workspace *workspace() const;
    void init();

    bool socketEnabled() const;
    void setSocketEnabled(bool newSocketEnabled);

    RootSurfaceContainer *rootContainer() const;
    Output *getOutput(WOutput *output) const;

    int currentUserId() const;
    void setCurrentUserId(int uid);

    float animationSpeed() const;
    void setAnimationSpeed(float newAnimationSpeed);

    void addSocket(WSocket *socket);

    WXWayland *createXWayland();
    void removeXWayland(WXWayland *xwayland);

    WSocket *defaultWaylandSocket() const;
    WXWayland *defaultXWaylandSocket() const;

public Q_SLOTS:
    void activeSurface(SurfaceWrapper *wrapper);
    void activeSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason);

signals:
    void socketEnabledChanged();
    void keyboardFocusSurfaceChanged();
    void activatedSurfaceChanged();
    void primaryOutputChanged();
    void currentUserIdChanged();

    void animationSpeedChanged();
    void socketFileChanged();

private:
    void allowNonDrmOutputAutoChangeMode(WOutput *output);
    void enableOutput(WOutput *output);

    int indexOfOutput(WOutput *output) const;

    void setOutputProxy(Output *output);

    void updateLayerSurfaceContainer(SurfaceWrapper *surface);

    SurfaceWrapper *keyboardFocusSurface() const;
    void setKeyboardFocusSurface(SurfaceWrapper *newActivateSurface, Qt::FocusReason reason);
    SurfaceWrapper *activatedSurface() const;
    void setActivatedSurface(SurfaceWrapper *newActivateSurface);

    void setCursorPosition(const QPointF &position);

    bool startDemoClient();

    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat,
                          WSurface *watched,
                          QObject *surfaceItem,
                          QObject *,
                          QInputEvent *event) override;
    bool unacceptedEvent(WSeat *, QWindow *, QInputEvent *event) override;
    void destoryTaskSwitcher();

    static Helper *m_instance;

    // qtquick helper
    WOutputRenderWindow *m_renderWindow = nullptr;

    // wayland helper
    WServer *m_server = nullptr;
    WSocket *m_socket = nullptr;
    WSeat *m_seat = nullptr;
    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;

    // protocols
    qw_compositor *m_compositor = nullptr;
    WXWayland *m_xwayland = nullptr;
    WInputMethodHelper *m_inputMethodHelper = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WForeignToplevel *m_foreignToplevel = nullptr;
    ForeignToplevelV1 *m_treelandForeignToplevel = nullptr;

    // privaet data
    QList<Output *> m_outputList;

    QPointer<SurfaceWrapper> m_keyboardFocusSurface;
    QPointer<SurfaceWrapper> m_activatedSurface;
    QPointer<QQuickItem> m_taskSwitch;

    RootSurfaceContainer *m_surfaceContainer = nullptr;
    LayerSurfaceContainer *m_backgroundContainer = nullptr;
    LayerSurfaceContainer *m_bottomContainer = nullptr;
    Workspace *m_workspace = nullptr;
    LockScreen *m_lockScreen = nullptr;
    LayerSurfaceContainer *m_topContainer = nullptr;
    LayerSurfaceContainer *m_overlayContainer = nullptr;
    SurfaceContainer *m_popupContainer = nullptr;
    int m_currentUserId = -1;
    float m_animationSpeed = 1.0;

    QList<WXWayland *> m_xwaylands;
};

Q_DECLARE_OPAQUE_POINTER(RootSurfaceContainer *)
