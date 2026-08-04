// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wimagebuffer.h"
#include "wtools.h"

#include <QImage>
#include <QColorSpace>

extern "C" {
#include <wlr/types/wlr_buffer.h>
}

#include <cstddef>

WAYLIB_SERVER_BEGIN_NAMESPACE

static bool wimagebuffer_begin_data_ptr_access(wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
    auto *self = reinterpret_cast<WImageBufferImpl *>(
        reinterpret_cast<char *>(buffer) - offsetof(WImageBufferImpl, base));
    return self->begin_data_ptr_access(flags, data, format, stride);
}

static void wimagebuffer_end_data_ptr_access(wlr_buffer *buffer)
{
    auto *self = reinterpret_cast<WImageBufferImpl *>(
        reinterpret_cast<char *>(buffer) - offsetof(WImageBufferImpl, base));
    self->end_data_ptr_access();
}

static void wimagebuffer_destroy(wlr_buffer *buffer)
{
    auto *self = reinterpret_cast<WImageBufferImpl *>(
        reinterpret_cast<char *>(buffer) - offsetof(WImageBufferImpl, base));
    delete self;
}

const struct wlr_buffer_impl WImageBufferImpl::impl = {
    .begin_data_ptr_access = wimagebuffer_begin_data_ptr_access,
    .end_data_ptr_access = wimagebuffer_end_data_ptr_access,
    .destroy = wimagebuffer_destroy,
};


WImageBufferImpl::WImageBufferImpl(const QImage &bufferImage)
{
    auto newFormat = WTools::convertToDrmSupportedFormat(bufferImage.format());
    if (newFormat != bufferImage.format()) {
        image = bufferImage.convertedTo(newFormat);
    } else {
        image = bufferImage;
    }
}

WImageBufferImpl::~WImageBufferImpl()
{

}

bool WImageBufferImpl::begin_data_ptr_access(uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
    if (!image.constBits()) {
        return false;
    }
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
        return false;
    }
    *data = (void *)image.constBits();
    *format = WTools::toDrmFormat(image.format());
    *stride = image.bytesPerLine();
    return true;
}

void WImageBufferImpl::end_data_ptr_access()
{
   // This space is intentionally left blank
}

WAYLIB_SERVER_END_NAMESPACE
