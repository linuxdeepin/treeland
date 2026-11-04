// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/shortcut/shortcutcontroller.h"
#include "rep_treelandwindowtree_source.h"
#include "treelanddebugsocket.h"


#include <QPointF>
#include <QTimer>

class SurfaceWrapper;
class SurfaceContainer;
class WorkspaceModel;

class TreelandRemoteSource : public WindowTreeRemoteSource
{
    Q_OBJECT

public:
    explicit TreelandRemoteSource(QObject *parent = nullptr);
    ~TreelandRemoteSource() override;

    // False when construction gave up (no free socket name, bind or chmod
    // failed, or the source could not be published), which lets the caller
    // discard the object and retry later instead of caching a dead one.
    bool isPublished() const { return m_published; }

    QPointF cursorPosition() const override;
    TreelandInfo getTreelandInfo() override;

    // ---- access ----
    // Opens the debug socket to every local user for 30 minutes; every later
    // remote call pushes the window out again. Only a caller that already
    // passed the sealed socket can ask, and in a release build that is root alone:
    // mode 0000 blocks the compositor's own user too, only CAP_DAC_OVERRIDE passes.
    bool requestNoAuth() override;

    // ---- inspection ----
    QList<WindowInfo> getWindows() override;
    QList<ClientInfo> getClients() override;
    IdleInfo getIdleInfo() override;

    // ---- window control ----
    bool activateWindow(qint64 id) override;
    bool closeWindow(qint64 id) override;
    bool minimizeWindow(qint64 id) override;
    bool toggleMaximized(qint64 id) override;
    bool toggleFullscreen(qint64 id) override;
    bool moveWindow(qint64 id, int x, int y) override;
    bool resizeWindow(qint64 id, int w, int h) override;
    bool setWindowWorkspace(qint64 id, int workspaceId) override;

    // ---- input / event injection ----
    bool moveCursor(QPointF pos) override;
    bool sendPointerButton(int button, bool pressed) override;
    bool sendKey(int qtKey, bool pressed) override;

    // ---- image capture (asynchronous) ----
    // Starts an async grab; the encoded PNG bytes are delivered via the
    // captureResult signal. The compositor main thread is never blocked.
    void captureOutput(QString outputName) override;
    void captureWindow(qint64 id) override;

    // ---- real-time monitoring ----
    QList<DebugEvent> getEvents(quint64 afterSeq) override;
    // ---- shortcut registry ----
    QList<ShortcutInfo> getShortcuts() override;
    qint64 focusedWindowId() override;
    qint64 windowUnderCursor() override;
    // ---- QtQuick scene inspection ----
    // Dumps the QQuickItem hierarchy of a window (id != 0) or the whole
    // render window (id == 0) as a pre-formatted text tree.
    QString getSceneTree(qint64 id) override;

protected:
    // Observes input events delivered to the render window. Installed lazily
    // only while a treeland-debug client is actively polling getEvents(), so
    // there is zero per-event overhead when nobody is monitoring.
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    // Feed the shortcut registration/trigger stream into the event ring
    // buffer while capture is armed.
    void onShortcutRegistered(const ShortcutController::Binding &binding);
    void onShortcutUnregistered(const QString &name);
    void onShortcutTriggered(ShortcutAction action, const QString &name, bool isGesture,
                             ShortcutController::KeyFlags keyFlags);

private:
    void collectSurfaceInfos(QList<WindowInfo> &infos,
                             SurfaceWrapper *surface,
                             int layer,
                             const QString &containerName,
                             int z) const;
    void collectWorkspaceModelWindows(QList<WindowInfo> &infos,
                                      WorkspaceModel *workspaceModel,
                                      int layer,
                                      const QString &containerName) const;
    void collectCurrentWorkspaceModelWindows(QList<WindowInfo> &infos,
                                             WorkspaceModel *workspaceModel,
                                             int layer,
                                             const QString &containerName) const;
    WindowInfo buildWindowInfo(SurfaceWrapper *surface,
                               int layer,
                               const QString &containerName,
                               int z) const;
    LayerInfo buildLayerInfo(SurfaceContainer *container) const;

    // Collects every toplevel SurfaceWrapper (no parent surface) reachable from
    // the root container, in a stable display order. Used by the flat window
    // list, the client grouping and the id -> surface lookup.
    void collectAllToplevelSurfaces(QList<SurfaceWrapper *> &out) const;

    // Resolves a surface id (the wl_surface wl_resource id) to a live
    // SurfaceWrapper, or nullptr if no current surface matches.
    SurfaceWrapper *findSurfaceById(qint64 id) const;

    // Starts/stops capturing input events into m_events. Capturing is gated on
    // a live treeland-debug poller: it is armed on every getEvents() call and
    // torn down after a short idle interval, guaranteeing no overhead when no
    // client is connected.
    void startEventCapture();
    void stopEventCapture();
    void appendEvent(int type, qint64 target, const QString &detail);

    // Closes the no-auth window: the timer expired, or the source is being torn
    // down. Idempotent -- a closed socket and a 0700 directory are the target
    // state, not a transition. Rebinding the host is what revokes the
    // connections a peer opened while the window was open.
    void closeNoAuthWindow();
    // Binds the debug socket and publishes WindowTree on it. Used by the
    // constructor and by closeNoAuthWindow() when it has to drop peers.
    bool bindHost(const QString &socketPath);
    // Called by every remote method: a call is what keeps no-auth mode alive.
    void noteActivity() const;

    // Event ring buffer for real-time monitoring; populated only while a client
    // is actively polling getEvents().
    QList<DebugEvent> m_events;
    quint64 m_nextEventSeq = 1;
    static constexpr int MAX_EVENTS = 2000;
    bool m_eventCaptureActive = false;
    QTimer *m_eventIdleTimer = nullptr;
    // Controller whose shortcut signals feed the event ring buffer while
    // capture is armed (null when capture is off).
    ShortcutController *m_shortcutController = nullptr;
    // Held for the lifetime of the QRemoteObjectHost so the socket name
    // claimed in the constructor stays ours (Wayland-style flock lock file,
    // see TreelandDebugSocketLock).
    TreelandDebugSocketLock m_debugSocketLock;
    QString m_debugSocketPath;
    // Release builds are the only ones that restrict the socket, so they are
    // also the only ones the access policy has to touch.
    bool m_socketRootOnly = false;
    // Turns no-auth mode off after a call-free window, so a forgotten request
    // cannot leave the debug channel open.
    QTimer *m_noAuthIdleTimer = nullptr;
    // True while the socket is open to every local user, so the teardown knows
    // whether peers may be holding a connection that has to be revoked.
    bool m_noAuthWindowOpen = false;
    bool m_published = false;
};
