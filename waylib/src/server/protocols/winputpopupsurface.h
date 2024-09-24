// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wtoplevelsurface.h>
#include <qwbox.h>

QW_BEGIN_NAMESPACE
class qw_input_popup_surface_v2;
QW_END_NAMESPACE

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
    WInputPopupSurface(QW_NAMESPACE::qw_input_popup_surface_v2 *surface, WSurface *parentSurface, QObject *parent = nullptr);
    WSurface *surface() const override;
    QW_NAMESPACE::qw_input_popup_surface_v2 *handle() const;
    QRect getContentGeometry() const override;
    bool doesNotAcceptFocus() const override;
    bool isActivated() const override;
    WSurface *parentSurface() const override;

    QRect cursorRect() const;
    void sendCursorRect(QRect rect);
Q_SIGNALS:
    void cursorRectChanged();

public Q_SLOTS:
    bool checkNewSize(const QSize &size) override;

protected:
    ~WInputPopupSurface() override = default;
};
WAYLIB_SERVER_END_NAMESPACE
