// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qwglobal.h>

#include <QSurfaceFormat>
#include <QPointer>
#include <qpa/qplatformscreen.h>

QW_USE_NAMESPACE

struct wlr_output;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class Q_DECL_HIDDEN QWlrootsScreen : public QPlatformScreen
{
public:
    QWlrootsScreen(WOutput *output);

    WOutput *output() const;

    QRect geometry() const override;

    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;

    qreal devicePixelRatio() const override;
    qreal refreshRate() const override;

    Qt::ScreenOrientation nativeOrientation() const override;
    Qt::ScreenOrientation orientation() const override;

    QWindow *topLevelAt(const QPoint &) const override;

    QString name() const override;
    QString manufacturer() const override;
    QString model() const override;
    QString serialNumber() const override;

    QPlatformCursor *cursor() const override;
    SubpixelAntialiasingType subpixelAntialiasingTypeHint() const override;

    PowerState powerState() const override;
    void setPowerState(PowerState) override;

    QVector<Mode> modes() const override;
    int currentMode() const override;
    int preferredMode() const override;

private:
    void init();
    QImage::Format getFormat() const;
    inline wlr_output *handle() const;

    QImage::Format m_format;
    QPointer<WOutput> m_output;
};

WAYLIB_SERVER_END_NAMESPACE
