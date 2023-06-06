// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qpa/qplatformwindow.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN QWlrootsOutputWindow : public QPlatformWindow
{
public:
    QWlrootsOutputWindow(QWindow *window);

    void initialize() override;

    QPlatformScreen *screen() const override;
    void setGeometry(const QRect &rect) override;
    QRect geometry() const override;

    WId winId() const override;
    qreal devicePixelRatio() const override;

private:
    QMetaObject::Connection onScreenGeometryConnection;
};

WAYLIB_SERVER_END_NAMESPACE
