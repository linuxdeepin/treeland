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
    Q_PROPERTY(WSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem CONSTANT)
    Q_PROPERTY(QQuickItem* eventItem READ eventItem NOTIFY eventItemChanged)
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode NOTIFY resizeModeChanged FINAL)
    Q_PROPERTY(bool effectiveVisible READ effectiveVisible NOTIFY effectiveVisibleChanged FINAL)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags NOTIFY flagsChanged FINAL)
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
        RejectEvent = 0x2
    };
    Q_ENUM(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit WSurfaceItem(QQuickItem *parent = nullptr);
    ~WSurfaceItem();

    static WSurfaceItem *fromFocusObject(QObject *focusObject);

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

    WSurface *surface() const;
    void setSurface(WSurface *newSurface);

    QQuickItem *contentItem() const;
    QQuickItem *eventItem() const;

    ResizeMode resizeMode() const;
    void setResizeMode(ResizeMode newResizeMode);
    Q_INVOKABLE void resize(ResizeMode mode);

    bool effectiveVisible() const;

    Flags flags() const;
    void setFlags(const Flags &newFlags);

Q_SIGNALS:
    void surfaceChanged();
    void subsurfaceAdded(WSurfaceItem *item);
    void subsurfaceRemoved(WSurfaceItem *item);
    void resizeModeChanged();
    void effectiveVisibleChanged();
    void eventItemChanged();
    void flagsChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void focusInEvent(QFocusEvent *event) override;
    void releaseResources() override;

    Q_SLOT virtual void onSurfaceCommit();
    virtual void initSurface();
    virtual bool sendEvent(QInputEvent *event);

    virtual bool resizeSurface(const QSize &newSize);
    virtual QRectF getContentGeometry() const;
    virtual bool inputRegionContains(const QPointF &position) const;

private:
    W_PRIVATE_SLOT(void onHasSubsurfaceChanged())

    friend class EventItem;
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSurfaceItem*)
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSurfaceItem::Flags)
