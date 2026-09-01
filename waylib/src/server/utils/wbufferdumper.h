// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <QImage>
#include <QRegion>
#include <QString>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WBufferDumper
{
public:
    enum class DumpResult {
        Success,
        InvalidBuffer,
        TextureCreationFailed,
        TextureReadFailed,
        UnsupportedFormat,
        SaveFailed
    };

    static DumpResult dumpBufferToFile(wlr_buffer *buffer,
                                       wlr_renderer *renderer,
                                       const QString &filePath);

    static DumpResult dumpBufferToImage(wlr_buffer *buffer,
                                        wlr_renderer *renderer,
                                        QImage &outputImage);

    static QString dumpResultToString(DumpResult result);

    // WAYLIB_DUMP_BUFFERS=<dir> enables a per-frame dump session.
    // WAYLIB_DUMP_BUFFERS_SKIP / WAYLIB_DUMP_BUFFERS_MAX bound the file window.
    static bool sessionEnabled();
    static quint64 sessionFrame();
    static void beginOutputFrame();
    static bool shouldWriteFiles();
    static void logLine(const QString &line);
    static QString dumpNamed(wlr_buffer *buffer,
                             wlr_renderer *renderer,
                             const QString &stem,
                             const QRegion &overlay = {});
    static QString describeRegion(const QRegion &region);
    static QString escapeJson(const QString &text);
};

WAYLIB_SERVER_END_NAMESPACE
