// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wimagebuffer.h"

#include <QImage>
#include <QColorSpace>

extern "C" {
#include <pixman.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/xcursor.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

inline pixman_format_code_t toPixmanFormat(QImage::Format format, bool sRGB=0) {
    switch (format) {
    using enum QImage::Format;
    case Format_ARGB32_Premultiplied:
        return sRGB ? PIXMAN_a8r8g8b8_sRGB : PIXMAN_a8r8g8b8;
    case Format_RGB32:
        return PIXMAN_x8r8g8b8;
    case Format_RGBA8888_Premultiplied:
        return PIXMAN_r8g8b8a8;
    case Format_RGBA8888:
        return PIXMAN_r8g8b8x8;
    case Format_RGB30:
        return PIXMAN_x2r10g10b10;
    case Format_A2RGB30_Premultiplied:
        return PIXMAN_a2r10g10b10;
    case Format_BGR30:
        return PIXMAN_x2b10g10r10;
    case Format_A2BGR30_Premultiplied:
        return PIXMAN_a2b10g10r10;

    /* 24bpp formats */
    case Format_RGB888:
        return PIXMAN_r8g8b8;
    case Format_BGR888:
        return PIXMAN_b8g8r8;

    /* 16bpp formats */
    case Format_RGB16:
        return PIXMAN_r5g6b5;
    case Format_RGB555:
        return PIXMAN_x1r5g5b5;
    case Format_ARGB4444_Premultiplied:
        return PIXMAN_a4r4g4b4;
    case Format_RGB444:
        return PIXMAN_x4r4g4b4;

    /* 8bpp formats */
    case Format_Alpha8:
        return PIXMAN_a8;
    case Format_Indexed8:
        return PIXMAN_c8;
    case Format_Grayscale8:
        return PIXMAN_g8;
    }
    qFatal("Unkown Qimage format:%d convert to pixman:", format);
}

WImageBufferImpl::WImageBufferImpl(const QImage &bufferImage)
{
    image = bufferImage;
}

WImageBufferImpl::~WImageBufferImpl()
{

}

bool WImageBufferImpl::beginDataPtrAccess(uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	if (!image.bits()) {
		return false;
	}
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = (void *)image.constBits();
	*format = toPixmanFormat(image.format(), image.colorSpace()==QColorSpace::SRgb);
	*stride = image.bytesPerLine();
	return true;
}

void WImageBufferImpl::endDataPtrAccess()
{
   // This space is intentionally left blank
}

WAYLIB_SERVER_END_NAMESPACE
