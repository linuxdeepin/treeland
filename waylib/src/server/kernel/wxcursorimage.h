// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QPoint>

struct wlr_xcursor;

QT_BEGIN_NAMESPACE
class QImage;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXCursorImagePrivate;
class WAYLIB_SERVER_EXPORT WXCursorImage : public WObject
{
    W_DECLARE_PRIVATE(WXCursorImage)

public:
    explicit WXCursorImage(const wlr_xcursor *cursor, float scale);
    explicit WXCursorImage(const QImage &image, const QPoint &hotspot);

    QPoint hotspot() const;
    QImage image() const;

    float scale() const;

    bool jumpToNextImage();
    bool jumpToImage(int imageNumber);
    int imageCount() const;
    int nextImageDelay() const;
    int currentImageNumber() const;
};

WAYLIB_SERVER_END_NAMESPACE
