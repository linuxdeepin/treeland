// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wimagebuffer.h"
#include <wcontainerof.h>
#include "wtools.h"

#include <QImage>
#include <QColorSpace>

WAYLIB_SERVER_BEGIN_NAMESPACE

WImageBufferImpl *WImageBufferImpl::fromBuffer(wlr_buffer *buffer)
{
    if (buffer->impl != &WImageBufferImpl::impl)
        return nullptr;
    return W_CONTAINER_OF(buffer, WImageBufferImpl, buffer);
}

wlr_buffer *WImageBufferImpl::create(const QImage &bufferImage)
{
    return (new WImageBufferImpl(bufferImage))->handle();
}

const struct wlr_buffer_impl WImageBufferImpl::impl = {
    .destroy = WImageBufferImpl::destroy,
    .get_dmabuf = NULL,
    .get_shm = WImageBufferImpl::get_shm,
    .begin_data_ptr_access = WImageBufferImpl::begin_data_ptr_access,
    .end_data_ptr_access = WImageBufferImpl::end_data_ptr_access,
};

WImageBufferImpl::WImageBufferImpl(const QImage &bufferImage)
    : buffer{}
    , image(new QImage(bufferImage))
{
    auto newFormat = WTools::convertToDrmSupportedFormat(bufferImage.format());
    if (newFormat != bufferImage.format()) {
        *image = image->convertedTo(newFormat);
    }
    wlr_buffer_init(&buffer, &impl, image->width(), image->height());
}

WImageBufferImpl::~WImageBufferImpl()
{
    delete image;
}

bool WImageBufferImpl::begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
                                             void **data, uint32_t *format, size_t *stride)
{
    auto *self = WImageBufferImpl::fromBuffer(buffer);
    Q_ASSERT(self);
    if (!self->image->constBits()) {
        return false;
    }
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
        return false;
    }
    *data = (void *)self->image->constBits();
    *format = WTools::toDrmFormat(self->image->format());
    *stride = self->image->bytesPerLine();
    return true;
}

bool WImageBufferImpl::get_shm(struct wlr_buffer * /*buffer*/, struct wlr_shm_attributes * /*attribs*/)
{
    // A QImage-backed buffer only supports data_ptr access; there is no
    // real SHM file descriptor to hand out. Faking fd=0 would make
    // wl_shm clients map the compositor's fd 0 as buffer memory.
    return false;
}

void WImageBufferImpl::end_data_ptr_access(struct wlr_buffer * /*buffer*/)
{
   // This space is intentionally left blank
}

void WImageBufferImpl::destroy(struct wlr_buffer *buffer)
{
    auto *self = WImageBufferImpl::fromBuffer(buffer);
    Q_ASSERT(self);
    // Standard wlroots release path: emits events.destroy, finishes the addon
    // set and asserts no listeners remain attached.
    wlr_buffer_finish(buffer);
    delete self;
}

WAYLIB_SERVER_END_NAMESPACE
