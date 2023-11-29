// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSurface>
#include <WLayerSurface>
#include <wtoplevelsurface.h>

QW_BEGIN_NAMESPACE
class QWSurface;
class QWXdgSurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WXdgSurfacePrivate;
class WAYLIB_SERVER_EXPORT WXdgSurface : public WToplevelSurface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgSurface)
    Q_PROPERTY(bool isPopup READ isPopup CONSTANT)
    Q_PROPERTY(bool isResizeing READ isResizeing NOTIFY resizeingChanged FINAL)
    Q_PROPERTY(WXdgSurface* parentXdgSurface READ parentXdgSurface NOTIFY parentXdgSurfaceChanged FINAL)
    QML_NAMED_ELEMENT(WaylandXdgSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WXdgSurface(QW_NAMESPACE::QWXdgSurface *handle, QObject *parent = nullptr);
    ~WXdgSurface();

    bool isPopup() const;
    bool doesNotAcceptFocus() const override;

    WSurface *surface() const override;
    QW_NAMESPACE::QWXdgSurface *handle() const;
    QW_NAMESPACE::QWSurface *inputTargetAt(QPointF &localPos) const;

    static WXdgSurface *fromHandle(QW_NAMESPACE::QWXdgSurface *handle);
    static WXdgSurface *fromSurface(WSurface *surface);

    WXdgSurface *parentXdgSurface() const;
    WSurface *parentSurface() const override;
    QPointF getPopupPosition() const;

    bool isResizeing() const;
    bool isActivated() const override;
    bool isMaximized() const override;
    bool isMinimized() const override;
    bool isFullScreen() const override;

    QRect getContentGeometry() const override;

    QSize minSize() const override;
    QSize maxSize() const override;

    QString title() const;
    QString appId() const;

public Q_SLOTS:
    void setResizeing(bool resizeing) override;
    void setMaximize(bool on) override;
    void setMinimize(bool on) override;
    void setActivate(bool on) override;
    void setFullScreen(bool on) override;

    bool checkNewSize(const QSize &size) override;
    void resize(const QSize &size) override;

Q_SIGNALS:
    void parentXdgSurfaceChanged();
    void resizeingChanged();
    void titleChanged();
    void appIdChanged();
};

WAYLIB_SERVER_END_NAMESPACE
