// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QSize>

QT_BEGIN_NAMESPACE
class QSGTexture;
class QQuickWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WTextureHandle;
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

    explicit WTexture(WTextureHandle *handle);

    WTextureHandle *handle() const;
    void setHandle(WTextureHandle *handle);

    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    Type type() const;

    QSize size() const;
    QSGTexture *getSGTexture(QQuickWindow *window);
};

WAYLIB_SERVER_END_NAMESPACE
