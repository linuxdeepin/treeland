// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wimagebuffer.h"
#include "wtools.h"

#include <QImage>
#include <QColorSpace>

WAYLIB_SERVER_BEGIN_NAMESPACE

WImageBufferImpl::WImageBufferImpl(const QImage &bufferImage)
{
    auto newFormat = WTools::convertToDrmSupportedFormat(bufferImage.format());
    if (newFormat != bufferImage.format()) {
        image = bufferImage.convertedTo(newFormat);
    } else {
        image = bufferImage;
    }

    static const wlr_buffer_impl impl {
        .destroy = destroy,
        .get_dmabuf = nullptr,
        .get_shm = getShm,
        .begin_data_ptr_access = beginDataPtrAccess,
        .end_data_ptr_access = endDataPtrAccess,
    };
    wlr_buffer_init(&buffer, &impl, image.width(), image.height());
}

WImageBufferImpl::~WImageBufferImpl()
{

}

wlr_buffer *WImageBufferImpl::create(const QImage &bufferImage)
{
    return (new WImageBufferImpl(bufferImage))->handle();
}

WImageBufferImpl *WImageBufferImpl::fromBuffer(wlr_buffer *buffer)
{
    return reinterpret_cast<WImageBufferImpl *>(buffer);
}

wlr_buffer *WImageBufferImpl::handle()
{
    return &buffer;
}

void WImageBufferImpl::destroy(wlr_buffer *buffer)
{
    wlr_buffer_finish(buffer);
    delete fromBuffer(buffer);
}

bool WImageBufferImpl::getShm(wlr_buffer *buffer, wlr_shm_attributes *attributes)
{
    const auto *self = fromBuffer(buffer);
    attributes->fd = 0;
    attributes->format = WTools::toDrmFormat(self->image.format());
    attributes->width = self->image.width();
    attributes->height = self->image.height();
    attributes->stride = self->image.bytesPerLine();
    attributes->offset = 0;
    return true;
}

bool WImageBufferImpl::beginDataPtrAccess(wlr_buffer *buffer, uint32_t flags,
                                          void **data, uint32_t *format, size_t *stride)
{
    auto *self = fromBuffer(buffer);
    if (!self->image.constBits()) {
        return false;
    }
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
        return false;
    }
    *data = const_cast<uchar *>(self->image.constBits());
    *format = WTools::toDrmFormat(self->image.format());
    *stride = self->image.bytesPerLine();
    return true;
}

void WImageBufferImpl::endDataPtrAccess(wlr_buffer *)
{
   // This space is intentionally left blank
}

WAYLIB_SERVER_END_NAMESPACE
