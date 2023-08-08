// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSurface>
#include <qwglobal.h>

QW_BEGIN_NAMESPACE
class QWSurface;
class QWXdgSurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WXdgSurfacePrivate;
class WAYLIB_SERVER_EXPORT WXdgSurface : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgSurface)
    Q_PROPERTY(bool isPopup READ isPopup CONSTANT)
    Q_PROPERTY(bool isResizeing READ isResizeing NOTIFY resizeingChanged FINAL)
    Q_PROPERTY(bool isActivated READ isActivated NOTIFY activateChanged FINAL)
    Q_PROPERTY(WSurface* surface READ surface CONSTANT FINAL)
    Q_PROPERTY(WXdgSurface* parentXdgSurface READ parentXdgSurface NOTIFY parentXdgSurfaceChanged FINAL)
    QML_NAMED_ELEMENT(WaylandXdgSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WXdgSurface(QW_NAMESPACE::QWXdgSurface *handle, QObject *parent = nullptr);
    ~WXdgSurface();

    bool isPopup() const;
    bool doesNotAcceptFocus() const;

    WSurface *surface() const;
    QW_NAMESPACE::QWXdgSurface *handle() const;
    QW_NAMESPACE::QWSurface *inputTargetAt(QPointF &localPos) const;

    static WXdgSurface *fromHandle(QW_NAMESPACE::QWXdgSurface *handle);
    static WXdgSurface *fromSurface(WSurface *surface);

    WXdgSurface *parentXdgSurface() const;

    bool isResizeing() const;
    bool isActivated() const;

    QRect getContentGeometry() const;

    QSize minSize() const;
    QSize maxSize() const;

public Q_SLOTS:
    void setResizeing(bool resizeing);
    void setMaximize(bool on);
    void setActivate(bool on);

    bool checkNewSize(const QSize &size);
    void resize(const QSize &size);

Q_SIGNALS:
    void parentXdgSurfaceChanged();
    void resizeingChanged();
    void activateChanged();
    void requestMove(WSeat *seat, quint32 serial);
    void requestResize(WSeat *seat, Qt::Edges edge, quint32 serial);
    void requestMaximize();
    void requestFullscreen();
    void requestToNormalState();
    void requestShowWindowMenu(WSeat *seat, QPoint pos, quint32 serial);
};

WAYLIB_SERVER_END_NAMESPACE
