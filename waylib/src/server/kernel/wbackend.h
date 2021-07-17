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

#include <WServer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputLayout;
class WRendererHandle;
class WBackendPrivate;
class WBackend : public WServerInterface, public WObject
{
    friend class WOutputPrivate;
    W_DECLARE_PRIVATE(WBackend)
public:
    WBackend(WOutputLayout *layout);

    WRendererHandle *renderer() const;
    template<typename DNativeInterface>
    DNativeInterface *rendererNativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(renderer());
    }

    QVector<WOutput*> outputList() const;
    QVector<WInputDevice*> inputDeviceList() const;

    WServer *server() const;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
};

WAYLIB_SERVER_END_NAMESPACE
