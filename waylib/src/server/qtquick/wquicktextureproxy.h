// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickTextureProxyPrivate;
class WAYLIB_SERVER_EXPORT WQuickTextureProxy : public QQuickItem, public WObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged FINAL)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged FINAL)
    Q_PROPERTY(bool hideSource READ hideSource WRITE setHideSource NOTIFY hideSourceChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY mipmapChanged)
    W_DECLARE_PRIVATE(WQuickTextureProxy)
    QML_NAMED_ELEMENT(TextureProxy)

public:
    explicit WQuickTextureProxy(QQuickItem *parent = nullptr);
    ~WQuickTextureProxy() override;

    QQuickItem* sourceItem() const;
    void setSourceItem(QQuickItem* sourceItem);

    QRectF sourceRect() const;
    void setSourceRect(const QRectF &sourceRect);

    bool hideSource() const;
    void setHideSource(bool newHideSource);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

    bool mipmap() const;
    void setMipmap(bool newMipmap);

Q_SIGNALS:
    void sourceItemChanged();
    void sourceRectChanged();
    void fixedChanged();
    void hideSourceChanged();
    void mipmapChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
};

WAYLIB_SERVER_END_NAMESPACE
