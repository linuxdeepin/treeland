// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QImage>

WAYLIB_SERVER_BEGIN_NAMESPACE

extern "C" {
#include <wlr/types/wlr_buffer.h>
}

#include <cstddef>

class WAYLIB_SERVER_EXPORT WImageBufferImpl
{
public:
    explicit WImageBufferImpl(const QImage &bufferImage);
    ~WImageBufferImpl();

    struct wlr_buffer base;

    bool begin_data_ptr_access(uint32_t flags, void **data, uint32_t *format, size_t *stride);
    void end_data_ptr_access();

    static const struct wlr_buffer_impl impl;

private:
    QImage image;
};

WAYLIB_SERVER_END_NAMESPACE
