/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wtexture.h"
#include "utils/wtools.h"

extern "C" {
#include <wlr/render/wlr_texture.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
}

#include <private/qsgplaintexture_p.h>
#include <private/qrhi_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WTexturePrivate : public WObjectPrivate {
public:
    WTexturePrivate(WTexture *qq, wlr_texture *handle);

    void init(wlr_texture *handle);
    void updateTexture() {
        if (!window)
            return;

        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(handle, &attribs);

#if QT_VERSION_MAJOR < 6
        texture->setTextureId(attribs.tex);
#else
        texture->setTextureFromNativeTexture(QQuickWindowPrivate::get(window)->rhi,
                                             attribs.tex, 0, QSize(handle->width, handle->height),
                                             {}, {});
#endif
        texture->setHasAlphaChannel(attribs.has_alpha);
        texture->setTextureSize(QSize(handle->width, handle->height));
    }

    void updateImage() {
        auto image = wlr_pixman_texture_get_image(handle);
        texture->setImage(WTools::fromPixmanImage(image));
    }

    W_DECLARE_PUBLIC(WTexture)

    wlr_texture *handle;
    WTexture::Type type;

    QScopedPointer<QSGPlainTexture> texture;
    void(WTexturePrivate::*onWlrTextureChanged)();

    QQuickWindow *window = nullptr;
};

WTexturePrivate::WTexturePrivate(WTexture *qq, wlr_texture *handle)
    : WObjectPrivate(qq)
    , handle(handle)
{
    if (handle)
        init(handle);
}

void WTexturePrivate::init(wlr_texture *handle)
{
    auto gpuTexture = new QSGPlainTexture();
    gpuTexture->setOwnsTexture(false);
    texture.reset(gpuTexture);

    if (wlr_texture_is_gles2(handle)) {
        type = WTexture::Type::Texture;
        onWlrTextureChanged = &WTexturePrivate::updateTexture;
    } else if (wlr_texture_is_pixman(handle)) {
        type = WTexture::Type::Image;
        onWlrTextureChanged = &WTexturePrivate::updateImage;
    } else {
        type = WTexture::Type::Unknow;
    }
}

WTexture::WTexture(WTextureHandle *handle)
    : WObject(*new WTexturePrivate(this, reinterpret_cast<wlr_texture*>(handle)))
{

}

WTextureHandle *WTexture::handle() const
{
    W_DC(WTexture);
    return reinterpret_cast<WTextureHandle*>(d->handle);
}

void WTexture::setHandle(WTextureHandle *handle)
{
    W_D(WTexture);

    auto new_handle = reinterpret_cast<wlr_texture*>(handle);

    if (Q_UNLIKELY(!new_handle)) {
        d->handle = nullptr;
        return;
    }

    if (Q_UNLIKELY(!d->handle)) {
        d->init(new_handle);
    }

    d->handle = new_handle;
}

WTexture::Type WTexture::type() const
{
    W_DC(WTexture);
    return d->type;
}

QSize WTexture::size() const
{
    W_DC(WTexture);
    return QSize(d->handle->width, d->handle->height);
}

QSGTexture *WTexture::getSGTexture(QQuickWindow *window)
{
    W_D(WTexture);

    const auto oldWindow = d->window;
    d->window = window;
    if (Q_UNLIKELY(!d->texture || window != oldWindow)) {
        (d->*(d->onWlrTextureChanged))();
    }
    return d->texture.get();
}

WAYLIB_SERVER_END_NAMESPACE
