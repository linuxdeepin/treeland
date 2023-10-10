// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WLayerSurface;
class WQuickLayerShellPrivate;
class WAYLIB_SERVER_EXPORT WQuickLayerShell: public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickLayerShell)
    QML_NAMED_ELEMENT(LayerShell)

public:
    explicit WQuickLayerShell(QObject *parent = nullptr);

Q_SIGNALS:
    void surfaceAdded(WLayerSurface *surface);
    void surfaceRemoved(WLayerSurface *surface);

private:
    void create() override;
};

class WAYLIB_SERVER_EXPORT WLayerSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_PROPERTY(WLayerSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)

    QML_NAMED_ELEMENT(LayerSurfaceItem)

public:
    explicit WLayerSurfaceItem(QQuickItem *parent = nullptr);
    ~WLayerSurfaceItem();

    WLayerSurface *surface() const;
    void setSurface(WLayerSurface *surface);

Q_SIGNALS:
    void surfaceChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    bool resizeSurface(const QSize &newSize) override;
    QRectF getContentGeometry() const override;

private:
    QPointer<WLayerSurface> m_surface;
};

WAYLIB_SERVER_END_NAMESPACE
