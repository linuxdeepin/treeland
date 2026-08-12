// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wlr_all.h>
#include <QImage>

#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WImageBufferImpl
{
public:
    static wlr_buffer *create(const QImage &bufferImage);
    WImageBufferImpl(const QImage &bufferImage);
    ~WImageBufferImpl();

    wlr_buffer *handle() { return &buffer; }

private:
    static const struct wlr_buffer_impl impl;
    static bool get_shm(struct wlr_buffer *buffer, struct wlr_shm_attributes *attribs);
    static bool begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
                                      void **data, uint32_t *format, size_t *stride);
    static void end_data_ptr_access(struct wlr_buffer *buffer);
    static void destroy(struct wlr_buffer *buffer);

    static WImageBufferImpl *fromBuffer(wlr_buffer *buffer);

    // QImage is kept behind a raw pointer so this class stays standard-layout
    // (std::unique_ptr's layout is implementation-defined across GCC/clang),
    // allowing container_of recovery of the owner from the wlr_buffer.
    wlr_buffer buffer;
    QImage *image;
};

WAYLIB_SERVER_END_NAMESPACE
