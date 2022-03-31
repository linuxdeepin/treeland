/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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
        Texture
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
