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
    Q_PROPERTY(QPoint hotspot READ hotspot NOTIFY hotspotChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY imageSourceChanged)
    QML_NAMED_ELEMENT(OutputCursor)

public:
    explicit QuickOutputCursor(QObject *parent = nullptr);
    ~QuickOutputCursor();

    static QString imageProviderId();

    bool visible() const;
    bool isHardwareCursor() const;
    QPoint hotspot() const;
    QUrl imageSource() const;

Q_SIGNALS:
    void visibleChanged();
    void isHardwareCursorChanged();
    void hotspotChanged();
    void imageSourceChanged();

private:
    void setVisible(bool newVisible);
    void setIsHardwareCursor(bool newIsHardwareCursor);
    void setHotspot(QPoint newHotspot);
    void setTexture(wlr_texture *texture, const QPointF &position);
    void setImageSource(const QUrl &newImageSource);
    void setPosition(const QPointF &pos);
    void setDelegateItem(QQuickItem *item);

    QQuickItem *delegateItem = nullptr;
    wlr_texture *lastTexture = nullptr;
    QPointF lastCursorPosition;
    bool m_visible = false;
    bool m_isHardwareCursor = false;
    QPoint m_hotspot;
    QUrl m_imageSource;
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
