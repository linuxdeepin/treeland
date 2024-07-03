// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QSGTextureProvider>

QW_BEGIN_NAMESPACE
class QWTexture;
class QWBuffer;
QW_END_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
class WBufferTextureProvider : public QSGTextureProvider
{
    Q_OBJECT
public:
    virtual QW_NAMESPACE::QWTexture *qwTexture() const = 0;
    virtual QW_NAMESPACE::QWBuffer *qwBuffer() const = 0;
};
WAYLIB_SERVER_END_NAMESPACE
