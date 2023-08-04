// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <WSurface>

#include <QQuickItem>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
class WOutput;
class WQuickSurface;
class WSurfaceItemPrivate;
class WAYLIB_SERVER_EXPORT WSurfaceItem : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WSurfaceItem)
    Q_PROPERTY(WSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged REQUIRED)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem NOTIFY contentItemChanged)
    QML_NAMED_ELEMENT(SurfaceItem)

public:
    explicit WSurfaceItem(QQuickItem *parent = nullptr);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

    bool contains(const QPointF &localPos) const override;

    WSurface *surface() const;
    void setSurface(WSurface *newSurface);

    QQuickItem *contentItem() const;

Q_SIGNALS:
    void surfaceChanged();
    void contentItemChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    bool event(QEvent *event) override;

    void releaseResources() override;
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    W_PRIVATE_SLOT(void invalidateSceneGraph()) // Using by Qt library
    W_PRIVATE_SLOT(void updateSubsurfaceItem())
    W_PRIVATE_SLOT(void onHasSubsurfaceChanged())
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSurfaceItem*)
