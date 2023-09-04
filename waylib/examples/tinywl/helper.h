// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WXdgSurface>
#include <WSeat>
#include <WCursor>
#include <WSurfaceItem>
#include <WOutput>

Q_DECLARE_OPAQUE_POINTER(QWindow*)

struct wlr_output_event_request_state;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class Helper : public WSeatEventFilter {
    Q_OBJECT
    Q_PROPERTY(WXdgSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    void stopMoveResize();

    WXdgSurface *activatedSurface() const;

public Q_SLOTS:
    void startMove(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, int serial);
    void startResize(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, Qt::Edges edge, int serial);
    void cancelMoveResize(QQuickItem *shell);
    bool startDemoClient(const QString &socket);
    WSurface *getFocusSurfaceFrom(QObject *object);

    void allowNonDrmOutputAutoChangeMode(WOutput *output);

signals:
    void activatedSurfaceChanged();

private:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WXdgSurface *newActivate);
    void onOutputRequeseState(wlr_output_event_request_state *newState);

    QPointer<WXdgSurface> m_activateSurface;

    // for move resize
    QPointer<WXdgSurface> surface;
    QPointer<QQuickItem> surfaceShellItem;
    QPointer<QQuickItem> eventItem;
    WSeat *seat = nullptr;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfstartMoveResize;
    Qt::Edges resizeEdgets;
};
