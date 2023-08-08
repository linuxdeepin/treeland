// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WXdgSurface>
#include <WSeat>
#include <WCursor>
#include <WSurfaceItem>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class Helper : public WSeatEventFilter {
    Q_OBJECT
    Q_PROPERTY(WXdgSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    void stop();

    WXdgSurface *activatedSurface() const;

public Q_SLOTS:
    Q_SLOT void startMove(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, int serial);
    Q_SLOT void startResize(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, Qt::Edges edge, int serial);
    Q_SLOT bool startDemoClient(const QString &socket);
    Q_SLOT WSurface *getFocusSurfaceFrom(QObject *object);

signals:
    void activatedSurfaceChanged();

private:
    bool eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool eventFilter(WSeat *seat, WSurface *watched, QObject *surfaceItem, QInputEvent *event) override;
    bool ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WXdgSurface *newActivate);

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
