// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferdumper.h"
#include "wtools.h"

#include <QImage>
#include <QLoggingCategory>

extern "C" {
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(wlcBufferDumper, "waylib.server.bufferdumper", QtWarningMsg)

WBufferDumper::DumpResult WBufferDumper::dumpBufferToImage(wlr_buffer *buffer, 
                                                           wlr_renderer *renderer, 
                                                           QImage &outputImage)
{
    if (!buffer || !renderer) {
        qCWarning(wlcBufferDumper) << "Invalid buffer or renderer";
        return DumpResult::InvalidBuffer;
    }

    wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
    if (!texture) {
        qCWarning(wlcBufferDumper) << "Failed to create texture from buffer";
        return DumpResult::TextureCreationFailed;
    }

    uint32_t format = wlr_texture_preferred_read_format(texture);
    
    QImage::Format qImageFormat = WTools::toImageFormat(format);
    if (qImageFormat == QImage::Format_Invalid) {
        wlr_texture_destroy(texture);
        return DumpResult::UnsupportedFormat;
    }

    outputImage = QImage(texture->width, texture->height, qImageFormat);
    uint32_t stride = outputImage.bytesPerLine();

    wlr_texture_read_pixels_options options = {};
    options.data = outputImage.bits();
    options.format = format;
    options.stride = stride;

    if (!wlr_texture_read_pixels(texture, &options)) {
        qCWarning(wlcBufferDumper) << "Failed to read pixels from texture";
        wlr_texture_destroy(texture);
        return DumpResult::TextureReadFailed;
    }

    wlr_texture_destroy(texture);

    return DumpResult::Success;
}

WBufferDumper::DumpResult WBufferDumper::dumpBufferToFile(wlr_buffer *buffer, 
                                                          wlr_renderer *renderer,
                                                          const QString &filePath)
{
    QImage image;
    DumpResult result = dumpBufferToImage(buffer, renderer, image);
    
    if (result != DumpResult::Success) {
        return result;
    }

    if (!image.save(filePath)) {
        qCWarning(wlcBufferDumper) << "Failed to save image to" << filePath;
        return DumpResult::SaveFailed;
    }

    return DumpResult::Success;
}

QString WBufferDumper::dumpResultToString(DumpResult result)
{
    switch (result) {
    case DumpResult::Success:
        return "Success";
    case DumpResult::InvalidBuffer:
        return "Invalid buffer or renderer";
    case DumpResult::TextureCreationFailed:
        return "Failed to create texture from buffer";
    case DumpResult::TextureReadFailed:
        return "Failed to read pixels from texture";
    case DumpResult::UnsupportedFormat:
        return "Unsupported pixel format";
    case DumpResult::SaveFailed:
        return "Failed to save image file";
    default:
        return "Unknown error";
    }
}

WAYLIB_SERVER_END_NAMESPACE
