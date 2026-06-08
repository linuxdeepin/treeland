// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <WSurface>
#include <wtoplevelsurface.h>
#include <wtextureproviderprovider.h>

#include <QQuickItem>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfaceItemContentPrivate;
class WSGTextureProvider;
class WAYLIB_SERVER_EXPORT WSurfaceItemContent : public QQuickItem, public virtual WTextureProviderProvider
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WSurfaceItemContent)
    Q_PROPERTY(WSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(bool cacheLastBuffer READ cacheLastBuffer WRITE setCacheLastBuffer NOTIFY cacheLastBufferChanged FINAL)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged FINAL)
    // override property to readonly
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    // override property to readonly
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)
    Q_PROPERTY(QPoint bufferOffset READ bufferOffset NOTIFY bufferOffsetChanged FINAL)
    Q_PROPERTY(bool ignoreBufferOffset READ ignoreBufferOffset WRITE setIgnoreBufferOffset NOTIFY ignoreBufferOffsetChanged FINAL)
    Q_PROPERTY(QRectF bufferSourceRect READ bufferSourceRect NOTIFY bufferSourceRectChanged FINAL)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio NOTIFY devicePixelRatioChanged FINAL)
    Q_PROPERTY(qreal alphaModifier READ alphaModifier NOTIFY alphaModifierChanged FINAL)
    QML_NAMED_ELEMENT(SurfaceItemContent)

public:
    explicit WSurfaceItemContent(QQuickItem *parent = nullptr);
    ~WSurfaceItemContent();

    WSurface *surface() const;
    void setSurface(WSurface *surface);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    WSGTextureProvider *wTextureProvider() const override;
    WOutputRenderWindow *outputRenderWindow() const override;

    bool cacheLastBuffer() const;
    void setCacheLastBuffer(bool newCacheLastBuffer);

    bool live() const;
    void setLive(bool live);

    QPoint bufferOffset() const;

    bool ignoreBufferOffset() const;
    void setIgnoreBufferOffset(bool newIgnoreBufferOffset);

    QRectF bufferSourceRect() const;
    qreal devicePixelRatio() const;
    qreal alphaModifier() const;

Q_SIGNALS:
    void surfaceChanged();
    void cacheLastBufferChanged();
    void liveChanged();
    void bufferOffsetChanged();
    void ignoreBufferOffsetChanged();
    void bufferSourceRectChanged();
    void devicePixelRatioChanged();
    void alphaModifierChanged();

private:
    friend class WSurfaceItem;
    friend class WSurfaceItemPrivate;
    friend class WSGTextureProvider;
    friend class WSGRenderFootprintNode;

    void componentComplete() override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void releaseResources() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

    // Using by Qt library
    Q_SLOT void invalidateSceneGraph();
};

class WCursor;
class WOutput;
class WQuickSurface;
class WSurfaceItemPrivate;
class WAYLIB_SERVER_EXPORT WSurfaceItem : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WSurfaceItem)
    Q_PROPERTY(WSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(WToplevelSurface* shellSurface READ shellSurface WRITE setShellSurface NOTIFY shellSurfaceChanged)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem NOTIFY contentItemChanged)
    Q_PROPERTY(QQuickItem* eventItem READ eventItem NOTIFY eventItemChanged)
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode NOTIFY resizeModeChanged FINAL)
    Q_PROPERTY(bool effectiveVisible READ effectiveVisible NOTIFY effectiveVisibleChanged FINAL)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags NOTIFY flagsChanged FINAL)
    Q_PROPERTY(qreal topPadding READ topPadding WRITE setTopPadding NOTIFY topPaddingChanged FINAL)
    Q_PROPERTY(qreal bottomPadding READ bottomPadding WRITE setBottomPadding NOTIFY bottomPaddingChanged FINAL)
    Q_PROPERTY(qreal leftPadding READ leftPadding WRITE setLeftPadding NOTIFY leftPaddingChanged FINAL)
    Q_PROPERTY(qreal rightPadding READ rightPadding WRITE setRightPadding NOTIFY rightPaddingChanged FINAL)
    // override property to readonly
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    // override property to readonly
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)
    Q_PROPERTY(qreal surfaceSizeRatio READ surfaceSizeRatio WRITE setSurfaceSizeRatio NOTIFY surfaceSizeRatioChanged)
    Q_PROPERTY(qreal bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY boundingRectChanged)
    Q_PROPERTY(bool subsurfacesVisible READ subsurfacesVisible WRITE setSubsurfacesVisible NOTIFY subsurfacesVisibleChanged FINAL)
    QML_NAMED_ELEMENT(SurfaceItem)

public:
    enum ResizeMode {
        SizeFromSurface,
        SizeToSurface,
        ManualResize
    };
    Q_ENUM(ResizeMode)

    enum Flag {
        DontCacheLastBuffer = 0x1,
        RejectEvent = 0x2,
        NonLive = 0x4,
        DelegateForSubsurface = 0x8,
    };
    Q_ENUM(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    enum class ZOrder : qint16 {
        BelowSubsurface = -100,
        ContentItem = 0,
        EventItem = 100,
        AboveSubsurface = 200
    };
    Q_ENUM(ZOrder)

    explicit WSurfaceItem(QQuickItem *parent = nullptr);
    ~WSurfaceItem();

    QRectF boundingRect() const override;

    static WSurfaceItem *fromFocusObject(QObject *focusObject);

    WSurface *surface() const;
    void setSurface(WSurface *newSurface);

    WToplevelSurface *shellSurface() const;
    virtual bool setShellSurface(WToplevelSurface *surface);

    QQuickItem *contentItem() const;
    QQuickItem *eventItem() const;

    ResizeMode resizeMode() const;
    void setResizeMode(ResizeMode newResizeMode);
    Q_INVOKABLE void resize(ResizeMode mode);

    bool effectiveVisible() const;

    Flags flags() const;
    void setFlags(const Flags &newFlags);

    qreal topPadding() const;
    void setTopPadding(qreal newTopPadding);

    qreal bottomPadding() const;
    void setBottomPadding(qreal newBottomPadding);

    qreal leftPadding() const;
    void setLeftPadding(qreal newLeftPadding);

    qreal rightPadding() const;
    void setRightPadding(qreal newRightPadding);

    qreal surfaceSizeRatio() const;
    void setSurfaceSizeRatio(qreal ssr);

    qreal bufferScale() const;

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *newDelegate);
    
    // resize internal surface, should be in SizeFromSurface mode
    bool resizeSurface(const QSizeF &newSize);
    bool subsurfacesVisible() const;
    void setSubsurfacesVisible(bool newSubsurfacesVisible);

Q_SIGNALS:
    void surfaceChanged();
    void subsurfaceAdded(WAYLIB_SERVER_NAMESPACE::WSurfaceItem *item);
    void subsurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WSurfaceItem *item);
    void resizeModeChanged();
    void effectiveVisibleChanged();
    void eventItemChanged();
    void flagsChanged();
    void topPaddingChanged();
    void bottomPaddingChanged();
    void leftPaddingChanged();
    void rightPaddingChanged();
    void surfaceSizeRatioChanged();
    void bufferScaleChanged();
    void contentItemChanged();
    void delegateChanged();
    void shellSurfaceChanged();
    void boundingRectChanged();
    void subsurfacesVisibleChanged();

protected:
    explicit WSurfaceItem(WSurfaceItemPrivate &dd, QQuickItem *parent = nullptr);
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void focusInEvent(QFocusEvent *event) override;
    void releaseResources() override;

    Q_SLOT virtual void onSurfaceCommit();
    virtual void initSurface();
    virtual bool sendEvent(QInputEvent *event);

    virtual bool doResizeSurface(const QSize &newSize);
    virtual QRectF getContentGeometry() const;
    virtual QSizeF getContentSize() const;
    virtual bool inputRegionContains(const QPointF &position) const;
    virtual void surfaceSizeRatioChange();

    void updateSurfaceState();

    friend class EventItem;
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSurfaceItem*)
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSurfaceItem::Flags)
