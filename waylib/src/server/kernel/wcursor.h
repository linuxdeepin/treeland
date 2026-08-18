// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <woutputlayout.h>
#include <wsurface.h>

#include <QPointF>
#include <QCursor>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevice;
class WXCursorImage;
class WCursorPrivate;
class WAYLIB_SERVER_EXPORT WCursor : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WCursor)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(QCursor cursor READ cursor WRITE setCursor NOTIFY cursorChanged FINAL)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurface* requestedDragSurface READ requestedDragSurface NOTIFY requestedDragSurfaceChanged FINAL)
    Q_PROPERTY(double scrollFactor READ scrollFactor WRITE setScrollFactor NOTIFY scrollFactorChanged FINAL)
    QML_ANONYMOUS

public:
    typedef WGlobal::CursorShape CursorShape;

    explicit WCursor(QObject *parent = nullptr);

    wlr_cursor *handle() const;

    static WCursor *fromHandle(wlr_cursor *handle);

    static Qt::MouseButton fromNativeButton(uint32_t code);
    static uint32_t toNativeButton(Qt::MouseButton button);
    static QCursor toQCursor(CursorShape shape);

    Qt::MouseButtons state() const;
    Qt::MouseButton button() const;

    WSeat *seat() const;
    QWindow *eventWindow() const;
    void setEventWindow(QWindow *window);

    static Qt::CursorShape defaultCursor();

    QCursor cursor() const;
    void setCursor(const QCursor &cursor);

    QCursor overrideCursor() const;
    void setOverrideCursor(const QCursor &cursor);

    // from client
    CursorShape requestedCursorShape() const;
    std::pair<WSurface*, QPoint> requestedCursorSurface() const;
    WSurface* requestedDragSurface() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void setPosition(const QPointF &pos);
    bool setPositionWithChecker(const QPointF &pos);

    bool isVisible() const;
    void setVisible(bool visible);

    // Active pointer constraint (locked/confined) enforced on this cursor, or
    // Pointer constraint enforcement — used by treeland
    // PointerConstraintsManager (the compositor policy layer).
    void setActivePointerConstraint(wlr_pointer_constraint_v1 *constraint);
    wlr_pointer_constraint_v1 *activePointerConstraint() const;
    // Warp to the constraint's cursor_hint before clearing it.
    // Internal: must be called before setActivePointerConstraint(nullptr).
    void warpToActiveConstraintHint();

    QPointF position() const;
    QPointF lastPressedOrTouchDownPosition() const;

    double scrollFactor() const;
    void setScrollFactor(double factor);

Q_SIGNALS:
    void positionChanged();
    void seatChanged();
    void requestedCursorShapeChanged();
    void requestedCursorSurfaceChanged();
    void requestedDragSurfaceChanged();
    void layoutChanged();
    void cursorChanged();
    void visibleChanged();
    void scrollFactorChanged();

public:
    ~WCursor() override;

protected:
    WCursor(WCursorPrivate &dd, QObject *parent = nullptr);

    virtual void move(wlr_input_device *device, const QPointF &delta);
    virtual void setPosition(wlr_input_device *device, const QPointF &pos);
    virtual bool setPositionWithChecker(wlr_input_device *device, const QPointF &pos);
    virtual void setScalePosition(wlr_input_device *device, const QPointF &ratio);

private:
    friend class WSeat;
    friend class WSeatPrivate;
    void setSeat(WSeat *seat);
    bool attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);
};

WAYLIB_SERVER_END_NAMESPACE
