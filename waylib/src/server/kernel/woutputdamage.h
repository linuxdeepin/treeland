/*
 * Copyright (C) 2022 zkyd
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
#include <wtypes.h>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputDamagePrivate;
class WOutputDamageHandle;
class Q_DECL_EXPORT WOutputDamage : public QObject, public WObject
{
    Q_OBJECT;
    W_DECLARE_PRIVATE(WOutputDamage);

public:
    explicit WOutputDamage(WOutput *output);

    WOutputDamageHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WOutputDamage *fromHandle(const WOutputDamageHandle *handle);
    template<typename DNativeInterface>
    static inline WOutputDamage *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WOutputDamageHandle*>(handle));
    }

    void add(const QRegion &region);
    void addWhole();
    void addRect(const QRect &rect);

Q_SIGNALS:
    void frame();
};

WAYLIB_SERVER_END_NAMESPACE
