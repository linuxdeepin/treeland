// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSurface>
#include <qwglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WToplevelSurface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isActivated READ isActivated NOTIFY activateChanged)
    Q_PROPERTY(bool isMaximized READ isMaximized NOTIFY maximizeChanged)
    Q_PROPERTY(bool isMinimized READ isMinimized NOTIFY minimizeChanged)
    Q_PROPERTY(WSurface* surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(WSurface* parentSurface READ parentSurface NOTIFY parentSurfaceChanged)
    QML_NAMED_ELEMENT(ToplevelSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WToplevelSurface(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual bool doesNotAcceptFocus() const {
        return false;
    }

    virtual WSurface *surface() const {
        return nullptr;
    }

    virtual WSurface *parentSurface() const {
        return nullptr;
    }

    virtual bool isActivated() const {
        return false;
    }
    virtual bool isMaximized() const {
        return false;
    }
    virtual bool isMinimized() const {
        return false;
    }
    virtual bool isFullScreen() const {
        return false;
    }

    virtual QRect getContentGeometry() const = 0;

    virtual QSize minSize() const {
        return QSize();
    }
    virtual QSize maxSize() const {
        return QSize();
    }

    virtual int keyboardFocusPriority() const {
        // When a high-priority surface obtains keyboard focus
        // it prevents a low-priority surface obtaining focus.
        // Should always be 0 except layer surface
        return 0;
    }

public Q_SLOTS:
    virtual void setMaximize(bool on) {
        Q_UNUSED(on);
    }
    virtual void setMinimize(bool on) {
        Q_UNUSED(on);
    }
    virtual void setActivate(bool on) {
        Q_UNUSED(on);
    }
    virtual void setResizeing(bool on) {
        Q_UNUSED(on);
    }
    virtual void setFullScreen(bool on) {
        Q_UNUSED(on);
    }

    virtual bool checkNewSize(const QSize &size) = 0;
    virtual void resize(const QSize &size) {
        Q_UNUSED(size)
    }

Q_SIGNALS:
    void activateChanged();
    void maximizeChanged();
    void minimizeChanged();
    void surfaceChanged();
    void parentSurfaceChanged();
    void fullscreenChanged();

    void requestMove(WSeat *seat, quint32 serial);
    void requestResize(WSeat *seat, Qt::Edges edge, quint32 serial);
    void requestMaximize();
    void requestCancelMaximize();
    void requestMinimize();
    void requestCancelMinimize();
    void requestFullscreen();
    void requestCancelFullscreen();
    void requestShowWindowMenu(WSeat *seat, QPoint pos, quint32 serial);
};

WAYLIB_SERVER_END_NAMESPACE
