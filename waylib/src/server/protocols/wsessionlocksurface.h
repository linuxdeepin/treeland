// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwglobal.h"
#include "wglobal.h"
#include "woutput.h"
#include "wtoplevelsurface.h"

struct wlr_session_lock_surface_v1;

QW_BEGIN_NAMESPACE
class qw_session_lock_surface_v1;
QW_END_NAMESPACE


WAYLIB_SERVER_BEGIN_NAMESPACE

class WSessionLockSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSessionLockSurface : public WToplevelSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSessionLockSurface)
    Q_PROPERTY(WSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(WOutput *output READ output CONSTANT) // constant in wlr_layershell_v1

    QML_NAMED_ELEMENT(WSessionLockSurface)
    QML_UNCREATABLE("Only create in C++")
public:
    explicit WSessionLockSurface(qw_session_lock_surface_v1 *handle, QObject *parent = nullptr);
    ~WSessionLockSurface();

    bool hasCapability(Capability cap) const override;

    WSurface *surface() const override;

    qw_session_lock_surface_v1 *handle() const;
    wlr_session_lock_surface_v1 *nativeHandle() const;

    static WSessionLockSurface *fromHandle(qw_session_lock_surface_v1 *handle);
    static WSessionLockSurface *fromSurface(WSurface *surface);

    QRect getContentGeometry() const override;
    void resize(const QSize &size) override;
    int keyboardFocusPriority() const override;

    Q_INVOKABLE uint32_t configureSize(const QSize &newSize);

    WOutput *output() const;
Q_SIGNALS:
    void surfaceChanged();
public Q_SLOTS:
    bool checkNewSize(const QSize &size, QSize *clippedSize) override;
};

WAYLIB_SERVER_END_NAMESPACE
