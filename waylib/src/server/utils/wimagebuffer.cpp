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
