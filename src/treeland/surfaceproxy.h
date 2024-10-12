// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQuickItem>

class SurfaceWrapper;

class SurfaceProxy : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(SurfaceWrapper* surface READ surface WRITE setSurface NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius RESET resetRadius NOTIFY radiusChanged FINAL)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged FINAL)
    Q_PROPERTY(QSizeF maxSize READ maxSize WRITE setMaxSize NOTIFY maxSizeChanged FINAL)
    Q_PROPERTY(bool fullProxy READ fullProxy WRITE setFullProxy NOTIFY fullProxyChanged FINAL)
    QML_ELEMENT

public:
    explicit SurfaceProxy(QQuickItem *parent = nullptr);

    SurfaceWrapper *surface() const;
    void setSurface(SurfaceWrapper *newSurface);

    qreal radius() const;
    void setRadius(qreal newRadius);
    void resetRadius();

    bool live() const;
    void setLive(bool newLive);

    QSizeF maxSize() const;
    void setMaxSize(const QSizeF &newMaxSize);

    bool fullProxy() const;
    void setFullProxy(bool newFullProxy);

Q_SIGNALS:
    void surfaceChanged();
    void radiusChanged();
    void liveChanged();
    void maxSizeChanged();
    void fullProxyChanged();

private:
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;
    void updateProxySurfaceScale();
    void updateProxySurfaceTitleBarAndDecoration();
    void updateImplicitSize();
    void onSourceRadiusChanged();

    SurfaceWrapper *m_sourceSurface = nullptr;
    SurfaceWrapper *m_proxySurface = nullptr;
    QList<QMetaObject::Connection> m_sourceConnections;
    QQuickItem *m_shadow = nullptr;
    qreal m_radius = -1;
    bool m_live = true;
    bool m_fullProxy = false;
    QSizeF m_maxSize;
};
