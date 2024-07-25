// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwbufferinterface.h>
#include <QImage>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WImageBufferImpl : public QW_NAMESPACE::qw_buffer_interface
{
public:
    WImageBufferImpl(const QImage &bufferImage);
    ~WImageBufferImpl();

    QW_INTERFACE(begin_data_ptr_access, bool, uint32_t flags, void **data, uint32_t *format, size_t *stride);
    QW_INTERFACE(end_data_ptr_access, void);

private:
    QImage image;
};

WAYLIB_SERVER_END_NAMESPACE
