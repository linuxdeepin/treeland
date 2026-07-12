// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QImage>
#include <QString>

extern "C" {
struct wlr_buffer;
struct wlr_renderer;
}

QW_BEGIN_NAMESPACE
class qw_buffer;
class qw_renderer;
QW_END_NAMESPACE

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

    static DumpResult dumpBufferToFile(QW_NAMESPACE::qw_buffer *buffer,
                                       QW_NAMESPACE::qw_renderer *renderer,
                                       const QString &filePath);

    static DumpResult dumpBufferToImage(wlr_buffer *buffer, 
                                        wlr_renderer *renderer,
                                        QImage &outputImage);

    static DumpResult dumpBufferToImage(QW_NAMESPACE::qw_buffer *buffer,
                                        QW_NAMESPACE::qw_renderer *renderer,
                                        QImage &outputImage);

    static QString dumpResultToString(DumpResult result);
};

WAYLIB_SERVER_END_NAMESPACE
