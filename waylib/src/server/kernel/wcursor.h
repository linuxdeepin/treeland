// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutputlayout.h>

#include <qwglobal.h>
#include <QPointF>

QT_BEGIN_NAMESPACE
class QCursor;
class QQuickWindow;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWXCursorManager;
class QWInputDevice;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevice;
class WXCursorManager;
class WXCursorImage;
class WCursorHandle;
class WCursorPrivate;
class WCursor : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WCursor)

public:
    explicit WCursor(QObject *parent = nullptr);

    WCursorHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WCursor *fromHandle(const WCursorHandle *handle);
    template<typename DNativeInterface>
    static inline WCursor *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WCursorHandle*>(handle));
    }

    static Qt::MouseButton fromNativeButton(uint32_t code);
    Qt::MouseButtons state() const;
    Qt::MouseButton button() const;

    WSeat *seat() const;
    QQuickWindow *eventWindow() const;
    void setEventWindow(QQuickWindow *window);

    static Qt::CursorShape defaultCursor();

    void setXCursorManager(QW_NAMESPACE::QWXCursorManager *manager);
    void setCursor(const QCursor &cursor);
    WXCursorImage *getCursorImage(float scale) const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void setPosition(const QPointF &pos);
    bool setPositionWithChecker(const QPointF &pos);

    QPointF position() const;
    QPointF lastPressedPosition() const;

Q_SIGNALS:
    void cursorImageMaybeChanged();

protected:
    WCursor(WCursorPrivate &dd, QObject *parent = nullptr);

    virtual void move(QW_NAMESPACE::QWInputDevice *device, const QPointF &delta);
    virtual void setPosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos);
    virtual bool setPositionWithChecker(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos);
    virtual void setScalePosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &ratio);

private:
    friend class WSeat;
    void setSeat(WSeat *seat);
    bool attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    W_PRIVATE_SLOT(void updateCursorImage())
};

WAYLIB_SERVER_END_NAMESPACE
