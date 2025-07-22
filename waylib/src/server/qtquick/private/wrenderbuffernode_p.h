// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QSGRenderNode>
#include <QPointer>
#include <QImage>
#include <QSGDynamicTexture>

QT_BEGIN_NAMESPACE
class QQuickItem;
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputRenderWindow;
class WAYLIB_SERVER_EXPORT WRenderBufferNode : public QSGRenderNode {
public:
    inline QSizeF size() const {
        return m_size;
    }
    QSGTexture *texture() const;

    static WRenderBufferNode *createRhiNode(QQuickItem *item);
    static WRenderBufferNode *createSoftwareNode(QQuickItem *item);

    QRectF rect() const override;
    RenderingFlags flags() const override;

    void resize(const QSizeF &size);
    void setContentItem(QQuickItem *item);

    typedef void(*TextureChangedNotifer)(WRenderBufferNode *node, void *data);
    void setTextureChangedCallback(TextureChangedNotifer callback, void *data);
    inline void doNotifyTextureChanged() {
        if (!m_renderCallback)
            return;
        m_renderCallback(this, m_callbackData);
    }
    virtual QImage toImage() const { return QImage(); }

    WOutputRenderWindow *renderWindow() const;
    qreal effectiveDevicePixelRatio() const;

protected:
    WRenderBufferNode(QQuickItem *item, QSGTexture *texture);

    QPointer<QQuickItem> m_item;
    QPointer<QQuickItem> m_content;
    QSizeF m_size;
    QRectF m_rect;
    QScopedPointer<QSGTexture> m_texture;
    TextureChangedNotifer m_renderCallback = nullptr;
    void *m_callbackData = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
