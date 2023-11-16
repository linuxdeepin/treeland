// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutputlayout.h>
#include <wsurface.h>

#include <qwglobal.h>
#include <QPointF>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWXCursorManager;
class QWInputDevice;
class QWCursor;
class QWOutputCursor;
class QWSurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevice;
class WXCursorImage;
class WCursorPrivate;
class WCursor : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WCursor)

public:
    enum CursorShape {
        Default = Qt::CustomCursor + 1,
        Invalid,
        BottomLeftCorner,
        BottomRightCorner,
        TopLeftCorner,
        TopRightCorner,
        BottomSide,
        LeftSide,
        RightSide,
        TopSide,
        Grabbing,
        Xterm,
        Hand1,
        Watch,
        SWResize,
        SEResize,
        SResize,
        WResize,
        EResize,
        EWResize,
        NWResize,
        NWSEResize,
        NEResize,
        NESWResize,
        NSResize,
        NResize,
        AllScroll,
        Text,
        Pointer,
        Wait,
        ContextMenu,
        Help,
        Progress,
        Cell,
        Crosshair,
        VerticalText,
        Alias,
        Copy,
        Move,
        NoDrop,
        NotAllowed,
        Grab,
        ColResize,
        RowResize,
        ZoomIn,
        ZoomOut
    };
    Q_ENUM(CursorShape)
    static_assert(BottomLeftCorner > Default, "");

    explicit WCursor(QObject *parent = nullptr);

    QW_NAMESPACE::QWCursor *handle() const;

    static WCursor *fromHandle(const QW_NAMESPACE::QWCursor *handle);

    static Qt::MouseButton fromNativeButton(uint32_t code);
    static uint32_t toNativeButton(Qt::MouseButton button);
    static QCursor toQCursor(CursorShape shape);

    Qt::MouseButtons state() const;
    Qt::MouseButton button() const;

    WSeat *seat() const;
    QWindow *eventWindow() const;
    void setEventWindow(QWindow *window);

    static Qt::CursorShape defaultCursor();

    void setXCursorManager(QW_NAMESPACE::QWXCursorManager *manager);
    QCursor cursor() const;
    void setCursor(const QCursor &cursor);
    void setSurface(QW_NAMESPACE::QWSurface *surface, const QPoint &hotspot);
    void setCursorShape(CursorShape shape);
    void setDragSurface(WSurface *surface);
    WSurface* dragSurface() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void setPosition(const QPointF &pos);
    bool setPositionWithChecker(const QPointF &pos);

    bool isVisible() const;
    void setVisible(bool visible);

    QPointF position() const;
    QPointF lastPressedPosition() const;

Q_SIGNALS:
    void positionChanged();
    void dragSurfaceChanged();

protected:
    WCursor(WCursorPrivate &dd, QObject *parent = nullptr);

    virtual void move(QW_NAMESPACE::QWInputDevice *device, const QPointF &delta);
    virtual void setPosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos);
    virtual bool setPositionWithChecker(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos);
    virtual void setScalePosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &ratio);

private:
    friend class WSeat;
    friend class WSeatPrivate;
    void setSeat(WSeat *seat);
    bool attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    W_PRIVATE_SLOT(void updateCursorImage())
};

WAYLIB_SERVER_END_NAMESPACE
