// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wtoplevelsurface.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WInputPopupSurfacePrivate;
class WAYLIB_SERVER_EXPORT WInputPopupSurface : public WToplevelSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputPopupSurface)
    QML_NAMED_ELEMENT(WaylandInputPopupSurface)
    QML_UNCREATABLE("Only created in C++")

public:
    WInputPopupSurface(wlr_input_popup_surface_v2 *surface, WSurface *parentSurface);
    ~WInputPopupSurface() override;
    WSurface *surface() const override;
    wlr_input_popup_surface_v2 *handle() const;
    QRect getContentGeometry() const override;
    bool hasCapability(Capability cap) const override;
    bool isActivated() const override;
    WSurface *parentSurface() const override;

    QRect cursorRect() const;
    void sendCursorRect(QRect rect);
Q_SIGNALS:
    void cursorRectChanged();

public Q_SLOTS:
    bool checkNewSize(const QSize &size, QSize *clipedSize = nullptr) override;

protected:
};
WAYLIB_SERVER_END_NAMESPACE
