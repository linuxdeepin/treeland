// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wtoplevelsurface.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WXdgSurface : public WToplevelSurface
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WaylandXdgSurface)
    QML_UNCREATABLE("Cannot create WaylandXdgSurface from QML")

protected:
    explicit WXdgSurface(WToplevelSurfacePrivate &d, QObject *parent = nullptr);
    ~WXdgSurface();

public:
    virtual bool isInitialized() const = 0;
};

WAYLIB_SERVER_END_NAMESPACE
