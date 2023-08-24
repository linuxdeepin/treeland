// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "wtexture.h"

#include <qwoutput.h>
#include <qwtexture.h>

#include <QQuickTextureFactory>
#include <private/qquickitem_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class CursorTextureFactory : public QQuickTextureFactory
{
public:
    CursorTextureFactory(QWTexture *texture)
        : QQuickTextureFactory()
        , texture(texture)
    {

    }

    QSGTexture *createTexture(QQuickWindow *window) const override;
    QSize textureSize() const override {
        return QSize(texture->handle()->width, texture->handle()->height);
    }
    int textureByteCount() const override {
        const QSize size = textureSize();
        // ###(zccrs): Don't know byte count of wlr_texture
        return size.width() * size.height() * 4;
    }
    QImage image() const override;

private:
    QWTexture *texture;
};

class CursorProvider : public QQuickImageProvider
{
public:
    CursorProvider()
        : QQuickImageProvider(QQmlImageProviderBase::Texture)
    {

    }

    QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize) override;
};

class QuickOutputCursor : public QObject
{
    friend class WOutputViewportPrivate;
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool isHardwareCursor READ isHardwareCursor NOTIFY isHardwareCursorChanged)
    Q_PROPERTY(QPointF hotspot READ hotspot NOTIFY hotspotChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    QML_NAMED_ELEMENT(OutputCursor)

public:
    explicit QuickOutputCursor(QObject *parent = nullptr);
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
    void setVisible(bool newVisible);
    void setIsHardwareCursor(bool newIsHardwareCursor);
    void setHotspot(QPointF newHotspot);
    void setTexture(wlr_texture *texture);
    void setImageSource(const QUrl &newImageSource);
    void setSize(const QSizeF &newSize);
    void setSourceRect(const QRectF &newSourceRect);
    void setPosition(const QPointF &pos);
    void setDelegateItem(QQuickItem *item);

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

class WOutputViewportPrivate : public QQuickItemPrivate
{
public:
    WOutputViewportPrivate()
    {

    }
    ~WOutputViewportPrivate() {
        clearCursors();
    }

    inline WOutputRenderWindow *outputWindow() const {
        auto ow = qobject_cast<WOutputRenderWindow*>(window);
        Q_ASSERT(ow);
        return ow;
    }

    void initForOutput();

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    void updateImplicitSize();

    void setVisible(bool visible) override;

    void clearCursors();
    void updateCursors();

    W_DECLARE_PUBLIC(WOutputViewport)
    WOutput *output = nullptr;
    WQuickSeat *seat = nullptr;
    qreal devicePixelRatio = 1.0;
    QQmlComponent *cursorDelegate = nullptr;
    QList<QuickOutputCursor*> cursors;
    QMetaObject::Connection updateCursorsConnection;
};

WAYLIB_SERVER_END_NAMESPACE
