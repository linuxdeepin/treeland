// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <WOutput>
#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderBufferBlitterPrivate;
class Q_DECL_EXPORT WRenderBufferBlitter : public QQuickItem, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WRenderBufferBlitter)
    Q_PRIVATE_PROPERTY(WRenderBufferBlitter::d_func(), QQmlListProperty<QObject> data READ data DESIGNABLE false)
    Q_PROPERTY(QQuickItem* content READ content CONSTANT)
    Q_PROPERTY(bool offscreen READ offscreen WRITE setOffscreen NOTIFY offscreenChanged FINAL)
    QML_NAMED_ELEMENT(RenderBufferBlitter)

public:
    explicit WRenderBufferBlitter(QQuickItem *parent = nullptr);
    ~WRenderBufferBlitter();

    QQuickItem *content() const;

    bool offscreen() const;
    void setOffscreen(bool newOffscreen);

Q_SIGNALS:
    void offscreenChanged();

private Q_SLOTS:
    void invalidateSceneGraph();

private:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void itemChange(ItemChange, const ItemChangeData &) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void releaseResources() override;
};

WAYLIB_SERVER_END_NAMESPACE
