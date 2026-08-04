// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QImage>

extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WImageBufferImpl
{
public:
    static wlr_buffer *create(const QImage &bufferImage);

private:
    explicit WImageBufferImpl(const QImage &bufferImage);
    ~WImageBufferImpl();

    static void destroy(wlr_buffer *buffer);
    static bool getShm(wlr_buffer *buffer, wlr_shm_attributes *attributes);
    static bool beginDataPtrAccess(wlr_buffer *buffer, uint32_t flags,
                                   void **data, uint32_t *format, size_t *stride);
    static void endDataPtrAccess(wlr_buffer *buffer);
    static WImageBufferImpl *fromBuffer(wlr_buffer *buffer);

    wlr_buffer *handle();

    wlr_buffer buffer;
    QImage image;
};

WAYLIB_SERVER_END_NAMESPACE
