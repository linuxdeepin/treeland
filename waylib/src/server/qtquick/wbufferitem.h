// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtextureproviderprovider.h>
#include <qwglobal.h>

#include <QQuickItem>
#include <QVariant>

QW_BEGIN_NAMESPACE
class qw_buffer;
QW_END_NAMESPACE
Q_DECLARE_OPAQUE_POINTER(QW_NAMESPACE::qw_buffer*)

QT_BEGIN_NAMESPACE
class QSGTextureProvider;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WBufferItemPrivate;
class WSGTextureProvider;
class WOutputRenderWindow;

// Minimal buffer-backed item: accepts a qw_buffer and renders it; always keeps the last buffer.
class WAYLIB_SERVER_EXPORT WBufferItem : public QQuickItem, public virtual WTextureProviderProvider
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WBufferItem)
    Q_PROPERTY(QVariant buffer READ bufferVariant WRITE setBufferVariant NOTIFY bufferChanged FINAL)
    QML_NAMED_ELEMENT(BufferItem)

public:
    explicit WBufferItem(QQuickItem *parent = nullptr);
    ~WBufferItem() override;

    QW_NAMESPACE::qw_buffer *buffer() const;
    void setBuffer(QW_NAMESPACE::qw_buffer *buffer);
    QVariant bufferVariant() const;
    void setBufferVariant(const QVariant &buffer);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    WSGTextureProvider *wTextureProvider() const override;
    WOutputRenderWindow *outputRenderWindow() const override;

Q_SIGNALS:
    void bufferChanged();

private:
    friend class WSGTextureProvider;

    void componentComplete() override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void releaseResources() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    Q_SLOT void invalidateSceneGraph();
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WBufferItem*)
