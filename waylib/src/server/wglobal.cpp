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

#include "wglobal.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WObject::WObject(WObject *)
    : WObject(*new WObjectPrivate(this))
{

}

WObject::WObject(WObjectPrivate &dd, WObject *)
    : w_d_ptr(&dd)
{

}

WObject::~WObject()
{

}

WObjectPrivate::~WObjectPrivate()
{

}

WObjectPrivate::WObjectPrivate(WObject *qq)
    : q_ptr(qq)
{

}

WAYLIB_SERVER_END_NAMESPACE
