// Copyright (C) 2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickscreencopy_p.h"
#include <qwscreencopyv1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWScreenCopyManagerV1;

class WQuickScreenCopyManagerPrivate : public WObjectPrivate
{
public:
    WQuickScreenCopyManagerPrivate(WQuickScreenCopyManager *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickScreenCopyManager)
};

WQuickScreenCopyManager::WQuickScreenCopyManager(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickScreenCopyManagerPrivate(this), nullptr)
{

}

void WQuickScreenCopyManager::create()
{
    W_D(WQuickScreenCopyManager);
    WQuickWaylandServerInterface::create();
    QWScreenCopyManagerV1::create(server()->handle());
}

WAYLIB_SERVER_END_NAMESPACE
