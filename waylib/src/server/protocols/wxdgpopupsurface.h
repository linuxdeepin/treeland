// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WXDGPOPUPSURFACE_H
#define WXDGPOPUPSURFACE_H

#include "wxdgsurface.h"

QW_BEGIN_NAMESPACE
class qw_xdg_popup;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgPopupSurfacePrivate;
class WAYLIB_SERVER_EXPORT WXdgPopupSurface : public WXdgSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgPopupSurface)
    QML_NAMED_ELEMENT(WaylandXdgPopupSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WXdgPopupSurface(QW_NAMESPACE::qw_xdg_popup *handle, QObject *parent = nullptr);
    ~WXdgPopupSurface();

    bool hasCapability(Capability cap) const override;

    WSurface *surface() const override;
    QW_NAMESPACE::qw_xdg_popup *handle() const;
    QW_NAMESPACE::qw_surface *inputTargetAt(QPointF &localPos) const;

    static WXdgPopupSurface *fromHandle(QW_NAMESPACE::qw_xdg_popup *handle);
    static WXdgPopupSurface *fromSurface(WSurface *surface);

    WSurface *parentSurface() const override;
    QPointF getPopupPosition() const;

    QRect getContentGeometry() const override;
    bool checkNewSize(const QSize &size, QSize *clipedSize = nullptr) override;

public Q_SLOTS:
    void resize(const QSize &size) override;
    void close() override;

Q_SIGNALS:
    void parentXdgSurfaceChanged();
    void resizeingChanged();
    void reposition();
};

WAYLIB_SERVER_END_NAMESPACE

#endif // WXDGPOPUPSURFACE_H
