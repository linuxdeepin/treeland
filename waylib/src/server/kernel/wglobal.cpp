// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WObject::WObject(WObjectPrivate &dd, WObject *)
    : w_d_ptr(&dd)
{

}

WObject::~WObject()
{

}

WObjectPrivate *WObjectPrivate::get(WObject *qq)
{
    return qq->d_func();
}

WObjectPrivate::~WObjectPrivate()
{

}

WObjectPrivate::WObjectPrivate(WObject *qq)
    : q_ptr(qq)
{

}

WAYLIB_SERVER_END_NAMESPACE
