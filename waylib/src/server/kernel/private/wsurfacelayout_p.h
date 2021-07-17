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

#include "wsurfacelayout.h"

#include <QPointF>
#include <QHash>

WAYLIB_SERVER_BEGIN_NAMESPACE

class EventGrabber;
class WSurfaceLayoutPrivate : public WObjectPrivate
{
public:
    WSurfaceLayoutPrivate(WSurfaceLayout *qq)
        : WObjectPrivate(qq) {}

    void updateSurfaceOutput(WSurface *surface);

    EventGrabber *createEventGrabber(WSurface *surface, WSeat *seat);

    W_DECLARE_PUBLIC(WSurfaceLayout)

    struct SurfaceData {
        WSurface *surface = nullptr;
        QPointF pos;
        WOutput *output = nullptr;
    };

    WOutputLayout *outputLayout = nullptr;
    QVector<WSurface*> surfaceList;
    QHash<WSurface*, SurfaceData> surfaceDataMap;
};

WAYLIB_SERVER_END_NAMESPACE
