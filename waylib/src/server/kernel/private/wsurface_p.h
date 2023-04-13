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

#include "wsurface.h"
#include "wsignalconnector.h"

#include <QPointer>

struct wlr_surface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfacePrivate : public WObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq);
    ~WSurfacePrivate();

    // begin slot function
    void on_commit(void *);
    void on_client_commit(void *);
    // end slot function

    void connect();
    void updateOutputs();

    W_DECLARE_PUBLIC(WSurface)

    wlr_surface *handle = nullptr;
    QVector<WOutput*> outputs;
    QPointer<WOutput> attachedOutput;
    WSurfaceLayout *layout = nullptr;

    WSignalConnector sc;
};

WAYLIB_SERVER_END_NAMESPACE
