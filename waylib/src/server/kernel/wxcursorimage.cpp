// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxcursorimage.h"
#include "private/wglobal_p.h"

#include <qwxcursormanager.h>

#include <QImage>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXCursorImagePrivate : public WObjectPrivate
{
public:
    WXCursorImagePrivate(WXCursorImage *qq)
        : WObjectPrivate(qq)
    {

    }

    int currentImageNumber;
    const wlr_xcursor *cursor = nullptr;
    float scale = 1.0;
    QImage currentImage;
    QPoint hotspot;
};

WXCursorImage::WXCursorImage(const wlr_xcursor *cursor, float scale)
    : WObject(*new WXCursorImagePrivate(this))
{
    Q_ASSERT(cursor && cursor->image_count > 0);
    W_D(WXCursorImage);
    d->cursor = cursor;
    d->currentImageNumber = -1;
    d->scale = scale;
    jumpToNextImage();
}

WXCursorImage::WXCursorImage(const QImage &image, const QPoint &hotspot)
    : WObject(*new WXCursorImagePrivate(this))
{
    W_D(WXCursorImage);
    d->currentImage = image;
    d->hotspot = hotspot;
    d->scale = image.devicePixelRatio();
}

QPoint WXCursorImage::hotspot() const
{
    W_DC(WXCursorImage);

    return d->hotspot;
}

QImage WXCursorImage::image() const
{
    W_DC(WXCursorImage);
    return d->currentImage;
}

float WXCursorImage::scale() const
{
    W_DC(WXCursorImage);
    return d->scale;
}

bool WXCursorImage::jumpToNextImage()
{
    W_D(WXCursorImage);
    return jumpToImage(d->currentImageNumber + 1);
}

bool WXCursorImage::jumpToImage(int imageNumber)
{
    W_D(WXCursorImage);

    if (!d->cursor)
        return false;

    if (imageNumber >= d->cursor->image_count)
        return false;
    d->currentImageNumber = imageNumber;

    auto ximage = d->cursor->images[d->currentImageNumber];
    d->currentImage = QImage(ximage->buffer, ximage->width, ximage->height, QImage::Format_ARGB32_Premultiplied);
    d->currentImage.setDevicePixelRatio(d->scale);
    d->hotspot = QPoint(ximage->hotspot_x, ximage->hotspot_x);

    return true;
}

int WXCursorImage::imageCount() const
{
    W_DC(WXCursorImage);
    return d->cursor->image_count;
}

int WXCursorImage::nextImageDelay() const
{
    W_DC(WXCursorImage);

    if (!d->cursor)
        return 0;

    return d->cursor->images[d->currentImageNumber]->delay;
}

int WXCursorImage::currentImageNumber() const
{
    W_DC(WXCursorImage);

    if (!d->cursor)
        return 0;

    return d->currentImageNumber;
}

WAYLIB_SERVER_END_NAMESPACE
