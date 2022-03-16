/*
 * Copyright (C) 2021 ~ 2022 zkyd
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
#ifndef WTOOLS_H
#define WTOOLS_H

#include <wglobal.h>
#include <QImage>
#include <QRegion>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WTools
{
public:
    static QImage fromPixmanImage(void *image, void *data = nullptr);
    static QRegion fromPixmanRegion(void *region);
    static void toPixmanRegion(const QRegion &region, void *pixmanRegion);
    static QRect fromWLRBox(void *box);
    static void toWLRBox(const QRect &rect, void *box);
};

WAYLIB_SERVER_END_NAMESPACE

#endif // WTOOLS_H
