// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferdumper.h"
#include "wtools.h"
#include "wayliblogging.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QRecursiveMutex>
#include <QPainter>
#include <QStringList>
#include <QTextStream>

#include <wlr_all.h>
#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

QString dumpDir()
{
    static const QString dir = qEnvironmentVariable("WAYLIB_DUMP_BUFFERS");
    return dir;
}

int skipFrames()
{
    static const int skip = qEnvironmentVariableIntValue("WAYLIB_DUMP_BUFFERS_SKIP");
    return skip;
}

int maxFrames()
{
    static const int max = [] {
        const int value = qEnvironmentVariableIntValue("WAYLIB_DUMP_BUFFERS_MAX");
        return value > 0 ? value : 40;
    }();
    return max;
}

QRecursiveMutex &logMutex()
{
    static QRecursiveMutex mutex;
    return mutex;
}

quint64 g_frame = 0;
bool g_dirReady = false;

bool ensureDumpDir()
{
    if (g_dirReady)
        return true;
    if (dumpDir().isEmpty())
        return false;
    QDir dir;
    if (!dir.mkpath(dumpDir())) {
        qCWarning(lcWlBufferDumper) << "Failed to create dump dir" << dumpDir();
        return false;
    }
    g_dirReady = true;
    qCInfo(lcWlBufferDumper) << "Dumping buffers to" << dumpDir()
                             << "skip" << skipFrames() << "max" << maxFrames();
    return true;
}

QImage toRgb(QImage image)
{
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32
        && image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }
    return image;
}

} // namespace

WBufferDumper::DumpResult WBufferDumper::dumpBufferToImage(wlr_buffer *buffer,
                                                           wlr_renderer *renderer,
                                                           QImage &outputImage)
{
    if (!buffer || !renderer) {
        qCWarning(lcWlBufferDumper) << "Invalid buffer or renderer";
        return DumpResult::InvalidBuffer;
    }

    wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
    if (!texture) {
        qCWarning(lcWlBufferDumper) << "Failed to create texture from buffer";
        return DumpResult::TextureCreationFailed;
    }

    struct ReadFormat {
        uint32_t drm;
        QImage::Format image;
    };
    // GLES2's IMPLEMENTATION_COLOR_READ is typically GL_RGBA (DRM ABGR8888).
    // Asking for XRGB/ARGB and wrapping the bytes as Format_RGB32 treats
    // R,G,B,A as B,G,R,A and swaps red/blue in the dumped image.
    const ReadFormat candidates[] = {
        { DRM_FORMAT_ABGR8888, QImage::Format_RGBA8888 },
        { DRM_FORMAT_ARGB8888, QImage::Format_ARGB32_Premultiplied },
        { wlr_texture_preferred_read_format(texture), WTools::toImageFormat(wlr_texture_preferred_read_format(texture)) },
    };

    DumpResult result = DumpResult::UnsupportedFormat;
    for (const ReadFormat &candidate : candidates) {
        if (candidate.drm == DRM_FORMAT_INVALID
            || candidate.image == QImage::Format_Invalid) {
            continue;
        }
        QImage image(texture->width, texture->height, candidate.image);
        if (image.isNull())
            continue;
        wlr_texture_read_pixels_options options = {};
        options.data = image.bits();
        options.format = candidate.drm;
        options.stride = uint32_t(image.bytesPerLine());
        if (!wlr_texture_read_pixels(texture, &options))
            continue;
        outputImage = image.convertToFormat(QImage::Format_ARGB32);
        result = DumpResult::Success;
        break;
    }

    wlr_texture_destroy(texture);
    if (result != DumpResult::Success)
        qCWarning(lcWlBufferDumper) << "Failed to read pixels from texture";
    return result;
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
    default:
        return "Unknown error";
    }
}

bool WBufferDumper::sessionEnabled()
{
    return !dumpDir().isEmpty();
}

quint64 WBufferDumper::sessionFrame()
{
    QMutexLocker locker(&logMutex());
    return g_frame;
}

void WBufferDumper::beginOutputFrame()
{
    if (!sessionEnabled())
        return;
    QMutexLocker locker(&logMutex());
    ++g_frame;
}

bool WBufferDumper::shouldWriteFiles()
{
    QMutexLocker locker(&logMutex());
    if (!sessionEnabled() || g_frame == 0)
        return false;
    if (g_frame <= static_cast<quint64>(skipFrames()))
        return false;
    return (g_frame - static_cast<quint64>(skipFrames())) <= static_cast<quint64>(maxFrames());
}

void WBufferDumper::logLine(const QString &line)
{
    if (!shouldWriteFiles() || !ensureDumpDir())
        return;

    QMutexLocker locker(&logMutex());
    QFile file(dumpDir() + QStringLiteral("/frames.jsonl"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCWarning(lcWlBufferDumper) << "Failed to open" << file.fileName();
        return;
    }
    QTextStream stream(&file);
    stream << line << '\n';
}

QString WBufferDumper::dumpNamed(wlr_buffer *buffer,
                                 wlr_renderer *renderer,
                                 const QString &stem,
                                 const QRegion &overlay)
{
    if (!shouldWriteFiles() || !ensureDumpDir())
        return {};

    QImage image;
    const DumpResult result = dumpBufferToImage(buffer, renderer, image);
    if (result != DumpResult::Success) {
        qCWarning(lcWlBufferDumper) << "Dump" << stem << "failed:" << dumpResultToString(result);
        return {};
    }

    image = toRgb(image);
    const QString path = dumpDir() + QLatin1Char('/') + stem + QStringLiteral(".png");
    if (!image.save(path)) {
        qCWarning(lcWlBufferDumper) << "Failed to save" << path;
        return {};
    }

    if (!overlay.isEmpty()) {
        QImage marked = image.convertToFormat(QImage::Format_ARGB32);
        QPainter painter(&marked);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 0, 0, 90));
        for (const QRect &rect : overlay)
            painter.drawRect(rect);
        const QString overlayPath = dumpDir() + QLatin1Char('/') + stem + QStringLiteral("_flush.png");
        marked.save(overlayPath);
    }

    return path;
}

QString WBufferDumper::describeRegion(const QRegion &region)
{
    if (region.isEmpty())
        return QStringLiteral("[]");
    QStringList rects;
    rects.reserve(region.rectCount());
    for (const QRect &rect : region) {
        rects.append(QStringLiteral("[%1,%2,%3,%4]")
                         .arg(rect.x())
                         .arg(rect.y())
                         .arg(rect.width())
                         .arg(rect.height()));
    }
    return QStringLiteral("[%1]").arg(rects.join(QLatin1Char(',')));
}

QString WBufferDumper::escapeJson(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (QChar ch : text) {
        switch (ch.unicode()) {
        case '\\':
            out += QLatin1String("\\\\");
            break;
        case '"':
            out += QLatin1String("\\\"");
            break;
        case '\n':
            out += QLatin1String("\\n");
            break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

WAYLIB_SERVER_END_NAMESPACE
