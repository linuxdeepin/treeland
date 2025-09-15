// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wsurfaceitem.h>


WAYLIB_SERVER_BEGIN_NAMESPACE

class WSessionLockSurface;

class WAYLIB_SERVER_EXPORT WSessionLockSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SessionLockSurfaceItem)

public:
    explicit WSessionLockSurfaceItem(QQuickItem *parent = nullptr);
    ~WSessionLockSurfaceItem();

    WSessionLockSurface *sessionLockSurface() const;

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
};

WAYLIB_SERVER_END_NAMESPACE
