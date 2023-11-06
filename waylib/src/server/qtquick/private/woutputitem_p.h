// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputitem.h"

#include <qwoutput.h>
#include <qwtexture.h>

#include <QQmlEngine>
#include <QQuickTextureFactory>
#include <private/qquickitem_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputItemAttached : public QObject
{
    friend class WOutputItem;
    Q_OBJECT
    Q_PROPERTY(WOutputItem* item READ item NOTIFY itemChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WOutputItemAttached(QObject *parent = nullptr);

    WOutputItem *item() const;

Q_SIGNALS:
    void itemChanged();

private:
    void setItem(WOutputItem *positioner);

private:
    WOutputItem *m_positioner = nullptr;
};

class QuickOutputCursor : public QObject
{
    friend class WOutputItem;
    friend class WOutputItemPrivate;
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool isHardwareCursor READ isHardwareCursor NOTIFY isHardwareCursorChanged)
    Q_PROPERTY(QPointF hotspot READ hotspot NOTIFY hotspotChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    QML_NAMED_ELEMENT(OutputCursor)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit QuickOutputCursor(wlr_output_cursor *handle, QObject *parent = nullptr);
    ~QuickOutputCursor();

    static QString imageProviderId();

    bool visible() const;
    bool isHardwareCursor() const;
    QPointF hotspot() const;
    QUrl imageSource() const;
    QSizeF size() const;
    QRectF sourceRect() const;

Q_SIGNALS:
    void visibleChanged();
    void isHardwareCursorChanged();
    void hotspotChanged();
    void imageSourceChanged();
    void sizeChanged();
    void sourceRectChanged();

private:
    void setHandle(wlr_output_cursor *handle);
    void setVisible(bool newVisible);
    void setIsHardwareCursor(bool newIsHardwareCursor);
    void setHotspot(QPointF newHotspot);
    void setTexture(wlr_texture *texture);
    void setImageSource(const QUrl &newImageSource);
    void setSize(const QSizeF &newSize);
    void setSourceRect(const QRectF &newSourceRect);
    bool setPosition(const QPointF &pos);
    void setDelegateItem(QQuickItem *item);

    wlr_output_cursor *m_handle = nullptr;
    QQuickItem *delegateItem = nullptr;
    wlr_texture *lastTexture = nullptr;
    bool m_visible = false;
    bool m_isHardwareCursor = false;
    QPointF m_hotspot;
    QUrl m_imageSource;
    QSizeF m_size;
    QRectF m_sourceRect;

    struct TextureAttrib {
        TextureAttrib (): type(INVALID) {}
        enum { GLES, PIXMAN, VULKAN, EMPTY, INVALID } type;
        union {
            GLuint tex;
            pixman_image_t *pimage;
#ifdef ENABLE_VULKAN_RENDER
            VkImage vimage;
#endif
        };
        bool operator==(const TextureAttrib& rhs) const {
            if (type != rhs.type)
                return false;
            switch (type) {
            case GLES:
                return tex == rhs.tex;
            case PIXMAN:
                return pimage == rhs.pimage;
#ifdef ENABLE_VULKAN_RENDER
            case VULKAN:
                return vimage == rhs.vimage;
#endif
            case EMPTY:
                return true;
            default:
                return false;
            }
        }
    } lastTextureAttrib;
};

WAYLIB_SERVER_END_NAMESPACE
