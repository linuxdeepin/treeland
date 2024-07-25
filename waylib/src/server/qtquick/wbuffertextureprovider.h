// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QSGTextureProvider>

QW_BEGIN_NAMESPACE
class qw_texture;
class qw_buffer;
QW_END_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
class WAYLIB_SERVER_EXPORT WBufferTextureProvider : public QSGTextureProvider
{
    Q_OBJECT
public:
    virtual QW_NAMESPACE::qw_texture *qwTexture() const = 0;
    virtual QW_NAMESPACE::qw_buffer *qwBuffer() const = 0;
};
WAYLIB_SERVER_END_NAMESPACE
