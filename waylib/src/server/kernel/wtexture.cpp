// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtexture.h"
#include "utils/wtools.h"

#include <qwtexture.h>

extern "C" {
#include <wlr/render/wlr_texture.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
}

#include <private/qsgplaintexture_p.h>
#include <private/qrhi_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

static void updateGLTexture(QWTexture *handle, QQuickWindow *window, QSGPlainTexture *texture) {
    wlr_gles2_texture_attribs attribs;
    wlr_gles2_texture_get_attribs(handle->handle(), &attribs);
    QSize size(handle->handle()->width, handle->handle()->height);

#define GL_TEXTURE_EXTERNAL_OES           0x8D65
    QQuickWindowPrivate::TextureFromNativeTextureFlags flags = attribs.target == GL_TEXTURE_EXTERNAL_OES
                                                                   ? QQuickWindowPrivate::NativeTextureIsExternalOES
                                                                   : QQuickWindowPrivate::TextureFromNativeTextureFlags {};
    texture->setTextureFromNativeTexture(QQuickWindowPrivate::get(window)->rhi,
                                         attribs.tex, 0, 0, size,
                                         {}, flags);

    texture->setHasAlphaChannel(attribs.has_alpha);
    texture->setTextureSize(size);
}

#ifdef ENABLE_VULKAN_RENDER
void updateVKTexture(QWTexture *handle, QQuickWindow *window, QSGPlainTexture *texture) {
    wlr_vk_image_attribs attribs;
    wlr_vk_texture_get_image_attribs(handle->handle(), &attribs);
    QSize size(handle->handle()->width, handle->handle()->height);

    texture->setTextureFromNativeTexture(QQuickWindowPrivate::get(window)->rhi,
                                         reinterpret_cast<quintptr>(attribs.image),
                                         attribs.layout, attribs.format, size,
                                         {}, {});
    texture->setHasAlphaChannel(wlr_vk_texture_has_alpha(handle->handle()));
    texture->setTextureSize(size);
}
#endif

void updateImage(QWTexture *handle, QQuickWindow *, QSGPlainTexture *texture) {
    auto image = wlr_pixman_texture_get_image(handle->handle());
    texture->setImage(WTools::fromPixmanImage(image));
}

typedef void(*UpdateTextureFunction)(QWTexture *, QQuickWindow *, QSGPlainTexture *);

static UpdateTextureFunction getUpdateTextFunction(QWTexture *handle, WTexture::Type &type)
{
    if (wlr_texture_is_gles2(handle->handle())) {
        type = WTexture::Type::GLTexture;
        return updateGLTexture;
    } else if (wlr_texture_is_pixman(handle->handle())) {
        type = WTexture::Type::Image;
        return updateImage;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_texture_is_vk(handle->handle())) {
        type = WTexture::Type::VKTexture;
        return updateVKTexture;
    }
#endif
    else {
        type = WTexture::Type::Unknow;
    }

    return nullptr;
}

class Q_DECL_HIDDEN WTexturePrivate : public WObjectPrivate {
public:
    WTexturePrivate(WTexture *qq, QWTexture *handle);

    inline wlr_texture *nativeHandle() const {
        Q_ASSERT(handle);
        return handle->handle();
    }

    void init(QWTexture *handle);

    W_DECLARE_PUBLIC(WTexture)

    QWTexture *handle;
    WTexture::Type type;
    bool ownsTexture = false;

    QScopedPointer<QSGPlainTexture> texture;
    UpdateTextureFunction updateTexture;

    QQuickWindow *window = nullptr;
};

WTexturePrivate::WTexturePrivate(WTexture *qq, QWTexture *handle)
    : WObjectPrivate(qq)
    , handle(handle)
    , updateTexture(nullptr)
{
    if (handle)
        init(handle);
}

void WTexturePrivate::init(QWTexture *new_handle)
{
    auto gpuTexture = new QSGPlainTexture();
    gpuTexture->setOwnsTexture(ownsTexture);
    texture.reset(gpuTexture);
    updateTexture = getUpdateTextFunction(new_handle, type);
}

WTexture::WTexture(QWTexture *handle)
    : WObject(*new WTexturePrivate(this, handle))
{

}

bool WTexture::makeTexture(QWTexture *handle, QSGPlainTexture *texture, QQuickWindow *window)
{
    Type type;
    auto updateTexture = getUpdateTextFunction(handle, type);
    if (!updateTexture)
        return false;
    updateTexture(handle, window, texture);
    return true;
}

QWTexture *WTexture::handle() const
{
    W_DC(WTexture);
    return d->handle;
}

void WTexture::setHandle(QWTexture *handle)
{
    W_D(WTexture);

    auto new_handle = reinterpret_cast<QWTexture*>(handle);

    if (Q_UNLIKELY(!new_handle)) {
        d->handle = nullptr;
        return;
    }

    if (Q_UNLIKELY(!d->handle)) {
        d->init(new_handle);
    }

    d->handle = new_handle;

    if (Q_LIKELY(d->updateTexture && d->window))
        d->updateTexture(d->handle, d->window, d->texture.get());
}

void WTexture::setOwnsTexture(bool owns)
{
    W_D(WTexture);
    d->ownsTexture = owns;
    if (d->texture)
        d->texture->setOwnsTexture(owns);
}

WTexture::Type WTexture::type() const
{
    W_DC(WTexture);
    return d->type;
}

QSize WTexture::size() const
{
    W_DC(WTexture);
    return QSize(d->nativeHandle()->width, d->nativeHandle()->height);
}

QSGTexture *WTexture::getSGTexture(QQuickWindow *window)
{
    W_D(WTexture);

    const auto oldWindow = d->window;
    d->window = window;
    if (Q_UNLIKELY(!d->texture || window != oldWindow)) {
        if (Q_LIKELY(d->updateTexture))
            d->updateTexture(d->handle, window, d->texture.get());
    }
    return d->texture.get();
}

const QImage &WTexture::image() const
{
    W_DC(WTexture);
    return d->texture->image();
}

WAYLIB_SERVER_END_NAMESPACE
