// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferdumper.h"
#include "wtools.h"
#include "wayliblogging.h"

#include <qwbuffer.h>
#include <qwrenderer.h>
#include <qwtexture.h>

#include <QImage>

#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE

WBufferDumper::DumpResult WBufferDumper::dumpBufferToImage(wlr_buffer *buffer, 
                                                           wlr_renderer *renderer, 
                                                           QImage &outputImage)
{
    return dumpBufferToImage(QW_NAMESPACE::qw_buffer::from(buffer),
                             QW_NAMESPACE::qw_renderer::from(renderer),
                             outputImage);
}

WBufferDumper::DumpResult WBufferDumper::dumpBufferToImage(QW_NAMESPACE::qw_buffer *buffer,
                                                           QW_NAMESPACE::qw_renderer *renderer,
                                                           QImage &outputImage)
{
    if (!buffer || !renderer) {
        qCWarning(lcWlBufferDumper) << "Invalid buffer or renderer";
        return DumpResult::InvalidBuffer;
    }

    std::unique_ptr<QW_NAMESPACE::qw_texture> texture(
        QW_NAMESPACE::qw_texture::from_buffer(*renderer, *buffer));
    if (!texture) {
        qCWarning(lcWlBufferDumper) << "Failed to create texture from buffer"
                                    << "buffer" << buffer
                                    << "renderer" << renderer;
        return DumpResult::TextureCreationFailed;
    }

    uint32_t format = texture->preferred_read_format();

    QImage::Format qImageFormat = WTools::toImageFormat(format);
    if (qImageFormat == QImage::Format_Invalid) {
        qCWarning(lcWlBufferDumper) << "Unsupported read format for buffer dump"
                                    << "format" << format
                                    << "buffer" << buffer;
        return DumpResult::UnsupportedFormat;
    }

    outputImage = QImage(texture->handle()->width, texture->handle()->height, qImageFormat);
    if (outputImage.isNull()) {
        qCWarning(lcWlBufferDumper) << "Failed to allocate image for buffer dump"
                                    << "size" << QSize(texture->handle()->width,
                                                       texture->handle()->height)
                                    << "format" << qImageFormat;
        return DumpResult::TextureReadFailed;
    }

    uint32_t stride = outputImage.bytesPerLine();

    wlr_texture_read_pixels_options options = {};
    options.data = outputImage.bits();
    options.format = format;
    options.stride = stride;

    if (!texture->read_pixels(&options)) {
        qCWarning(lcWlBufferDumper) << "Failed to read pixels from texture"
                                    << "texture" << texture.get()
                                    << "buffer" << buffer
                                    << "format" << format
                                    << "stride" << stride;
        return DumpResult::TextureReadFailed;
    }

    return DumpResult::Success;
}

WBufferDumper::DumpResult WBufferDumper::dumpBufferToFile(wlr_buffer *buffer, 
                                                          wlr_renderer *renderer,
                                                          const QString &filePath)
{
    return dumpBufferToFile(QW_NAMESPACE::qw_buffer::from(buffer),
                            QW_NAMESPACE::qw_renderer::from(renderer),
                            filePath);
}

WBufferDumper::DumpResult WBufferDumper::dumpBufferToFile(QW_NAMESPACE::qw_buffer *buffer,
                                                          QW_NAMESPACE::qw_renderer *renderer,
                                                          const QString &filePath)
{
    QImage image;
    DumpResult result = dumpBufferToImage(buffer, renderer, image);
    
    if (result != DumpResult::Success) {
        return result;
    }

    if (!image.save(filePath)) {
        qCWarning(lcWlBufferDumper) << "Failed to save image to" << filePath;
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
