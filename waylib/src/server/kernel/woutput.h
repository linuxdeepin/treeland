/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>
#include <wtypes.h>

#include <QObject>
#include <QSize>

QT_BEGIN_NAMESPACE
class QQuickWindow;
class QScreen;
class QWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickRenderControl;
class WInputEvent;
class WCursor;
class WOutputLayout;
class WBackend;
class WServer;
class WOutputHandle;
class WOutputPrivate;
class WAYLIB_SERVER_EXPORT WOutput : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutput)
    Q_PROPERTY(QSize size READ effectiveSize NOTIFY effectiveSizeChanged)
    Q_PROPERTY(Transform orientation READ orientation WRITE rotate NOTIFY orientationChanged)
    Q_PROPERTY(float scale READ scale WRITE setScale NOTIFY scaleChanged)

public:
    enum Transform {
        Normal = WLR::Transform::Normal,
        R90 = WLR::Transform::R90,
        R180 = WLR::Transform::R180,
        R270 = WLR::Transform::R270,
        Flipped = WLR::Transform::Flipped,
        Flipped90 = WLR::Transform::Flipped90,
        Flipped180 = WLR::Transform::Flipped180,
        Flipped270 = WLR::Transform::Flipped270
    };
    Q_ENUM(Transform)

    explicit WOutput(WOutputHandle *handle, WBackend *backend);
    ~WOutput();

    WBackend *backend() const;
    bool isValid() const;

    void attach(WQuickRenderControl *control);
    void detach();

    WOutputHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WOutput *fromHandle(const WOutputHandle *handle);
    template<typename DNativeInterface>
    static inline WOutput *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WOutputHandle*>(handle));
    }
    static WOutput *fromScreen(const QScreen *screen);
    static WOutput *fromWindow(const QWindow *window);

    void rotate(Transform t);
    void setScale(float scale);

    QPoint position() const;
    QSize size() const;
    QSize transformedSize() const;
    QSize effectiveSize() const;
    Transform orientation() const;
    float scale() const;

    QQuickWindow *attachedWindow() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void cursorEnter(WCursor *cursor);
    void cursorLeave(WCursor *cursor);
    QVector<WCursor*> cursorList() const;

    void postInputEvent(WInputEvent *event);
    WInputEvent *currentInputEvent() const;

    QCursor cursor() const;

public Q_SLOTS:
    void requestRender();
    void requestRenderCursor();

Q_SIGNALS:
    void positionChanged(const QPoint &pos);
    void sizeChanged();
    void transformedSizeChanged(const QSize &size);
    void effectiveSizeChanged(const QSize &size);
    void orientationChanged();
    void scaleChanged();

private:
    friend class WServer;
    friend class WServerPrivate;
};

WAYLIB_SERVER_END_NAMESPACE
