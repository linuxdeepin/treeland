// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QSize>

QT_BEGIN_NAMESPACE
class QSGTexture;
class QSGPlainTexture;
class QQuickWindow;
class QImage;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWTexture;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WTexturePrivate;
class WTexture : public WObject
{
    W_DECLARE_PRIVATE(WTexture)

public:
    enum class Type {
        Unknow,
        Image,
        GLTexture,
        VKTexture
    };

    explicit WTexture(QW_NAMESPACE::QWTexture *handle);

    static bool makeTexture(QW_NAMESPACE::QWTexture *handle, QSGPlainTexture *texture, QQuickWindow *window);

    QW_NAMESPACE::QWTexture *handle() const;
    void setHandle(QW_NAMESPACE::QWTexture *handle);
    void setOwnsTexture(bool owns);

    Type type() const;

    QSize size() const;
    QSGTexture *getSGTexture(QQuickWindow *window);

    const QImage &image() const;
};

WAYLIB_SERVER_END_NAMESPACE
