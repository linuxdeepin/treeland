// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwbufferinterface.h>
#include <QImage>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WImageBufferImpl : public QW_NAMESPACE::qw_buffer_interface<WImageBufferImpl>
{
public:
    WImageBufferImpl(const QImage &bufferImage);
    ~WImageBufferImpl();

    bool begin_data_ptr_access(uint32_t flags, void **data, uint32_t *format, size_t *stride);
    void end_data_ptr_access();

private:
    QImage image;
};

WAYLIB_SERVER_END_NAMESPACE
