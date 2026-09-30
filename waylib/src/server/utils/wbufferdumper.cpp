// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferdumper.h"
#include "wtools.h"
#include "wayliblogging.h"

#include <wpointer.h>
#include <wlr_all.h>

#include <QImage>
#include <QScopeGuard>

WAYLIB_SERVER_BEGIN_NAMESPACE

WBufferDumper::DumpResult WBufferDumper::dumpBufferToImage(wlr_buffer *buffer,
                                                           wlr_renderer *renderer,
                                                           QImage &outputImage)
{
    if (!buffer || !renderer) {
        qCWarning(lcWlBufferDumper) << "Invalid buffer or renderer";
        return DumpResult::InvalidBuffer;
    }

    const bool isVulkanRenderer = wlr_renderer_is_vk(renderer);

    // On the Vulkan renderer, buffers with a CPU representation (shared
    // memory client buffers, e.g. XWayland) are copied directly. This avoids
    // importing them as staging pixel textures, whose device layout
    // (SHADER_READ_ONLY_OPTIMAL after upload) upstream's vulkan_read_pixels()
    // does not account for.
    if (isVulkanRenderer) {
        void *data = nullptr;
        uint32_t shmFormat = 0;
        size_t shmStride = 0;
        if (wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                             &data, &shmFormat, &shmStride)) {
            QImage::Format mappedFormat = WTools::toImageFormat(shmFormat);
            if (mappedFormat != QImage::Format_Invalid) {
                QImage wrapped(static_cast<const uchar *>(data),
                               buffer->width, buffer->height,
                               int(shmStride), mappedFormat);
                if (!wrapped.isNull()) {
                    outputImage = wrapped.copy();
                    wlr_buffer_end_data_ptr_access(buffer);
                    return DumpResult::Success;
                }
            }
            wlr_buffer_end_data_ptr_access(buffer);
        }
    }

    WUniquePointer<wlr_texture> texture(wlr_texture_from_buffer(renderer, buffer));
    if (!texture) {
        qCWarning(lcWlBufferDumper) << "Failed to create texture from buffer"
                                    << "buffer" << buffer
                                    << "renderer" << renderer;
        return DumpResult::TextureCreationFailed;
    }

    uint32_t format = wlr_texture_preferred_read_format(texture.get());

    QImage::Format qImageFormat = WTools::toImageFormat(format);
    if (qImageFormat == QImage::Format_Invalid) {
        qCWarning(lcWlBufferDumper) << "Unsupported read format for buffer dump"
                                    << "format" << format
                                    << "buffer" << buffer;
        return DumpResult::UnsupportedFormat;
    }

    outputImage = QImage(texture->width, texture->height, qImageFormat);
    if (outputImage.isNull()) {
        qCWarning(lcWlBufferDumper) << "Failed to allocate image for buffer dump"
                                    << "size" << QSize(texture->width, texture->height)
                                    << "format" << qImageFormat;
        return DumpResult::TextureReadFailed;
    }

    uint32_t stride = outputImage.bytesPerLine();

    // On the Vulkan renderer a DMA-BUF import is FOREIGN-owned between
    // frames, so reading its pixels requires the wlroots-side acquire gate
    // (producer fence wait + queue-family ownership) before touching it, and
    // the matching release afterwards. Shared-memory buffers were already
    // handled by the CPU copy path above; NOOP covers the remaining cases.
    bool readbackAcquired = false;
    if (isVulkanRenderer) {
        const auto state = waylib_vk_texture_begin_readback(renderer, texture.get());
        switch (state) {
        case WLR_VK_TEXTURE_READBACK_ACQUIRED:
            readbackAcquired = true;
            break;
        case WLR_VK_TEXTURE_READBACK_NOOP:
            break;
        case WLR_VK_TEXTURE_READBACK_ERROR:
            qCWarning(lcWlBufferDumper) << "Vulkan texture readback acquire failed"
                                        << "texture" << texture.get()
                                        << "buffer" << buffer;
            return DumpResult::ReadbackSyncFailed;
        }
    }
    const auto readbackGuard = qScopeGuard([&texture, renderer, readbackAcquired] {
        if (readbackAcquired)
            waylib_vk_texture_end_readback(renderer, texture.get());
    });

    wlr_texture_read_pixels_options options = {};
    options.data = outputImage.bits();
    options.format = format;
    options.stride = stride;

    if (!wlr_texture_read_pixels(texture.get(), &options)) {
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
    case DumpResult::ReadbackSyncFailed:
        return "Failed to acquire Vulkan texture for readback";
    default:
        return "Unknown error";
    }
}

WAYLIB_SERVER_END_NAMESPACE
