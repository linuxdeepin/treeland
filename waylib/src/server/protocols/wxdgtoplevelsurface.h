// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WXDGTOPLEVELSURFACE_H
#define WXDGTOPLEVELSURFACE_H

#include "wxdgsurface.h"

QW_BEGIN_NAMESPACE
class qw_xdg_toplevel;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgToplevelSurfacePrivate;

class WAYLIB_SERVER_EXPORT WXdgToplevelSurface : public WXdgSurface
{    
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgToplevelSurface)
    Q_PROPERTY(bool isResizeing READ isResizeing NOTIFY resizeingChanged FINAL)
    Q_PROPERTY(WXdgSurface* parentXdgSurface READ parentXdgSurface NOTIFY parentXdgSurfaceChanged FINAL)
    QML_NAMED_ELEMENT(WaylandXdgToplevelSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WXdgToplevelSurface(QW_NAMESPACE::qw_xdg_toplevel *handle, QObject *parent = nullptr);
    ~WXdgToplevelSurface();

    bool hasCapability(Capability cap) const override;

    WSurface *surface() const override;
    QW_NAMESPACE::qw_xdg_toplevel *handle() const;
    QW_NAMESPACE::qw_surface *inputTargetAt(QPointF &localPos) const;

    static WXdgToplevelSurface *fromHandle(QW_NAMESPACE::qw_xdg_toplevel *handle);
    static WXdgToplevelSurface *fromSurface(WSurface *surface);

    WXdgToplevelSurface *parentXdgSurface() const;
    WSurface *parentSurface() const override;

    bool isResizeing() const;
    bool isActivated() const override;
    bool isMaximized() const override;
    bool isMinimized() const override;
    bool isFullScreen() const override;

    QRect getContentGeometry() const override;

    QSize minSize() const override;
    QSize maxSize() const override;

    QString title() const override;
    QString appId() const override;

    bool isInitialized() const override;

public Q_SLOTS:
    void setResizeing(bool resizeing) override;
    void setMaximize(bool on) override;
    void setMinimize(bool on) override;
    void setActivate(bool on) override;
    void setFullScreen(bool on) override;

    bool checkNewSize(const QSize &size, QSize *clipedSize = nullptr) override;
    void resize(const QSize &size) override;
    void close() override;

Q_SIGNALS:
    void parentXdgSurfaceChanged();
    void resizeingChanged();
};

WAYLIB_SERVER_END_NAMESPACE

#endif // WXDGTOPLEVELSURFACE_H
