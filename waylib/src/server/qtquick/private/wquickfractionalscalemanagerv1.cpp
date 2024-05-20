// Copyright (C) 2024 YaoBing Xiao <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "wquickfractionalscalemanagerv1_p.h"

#include <qwfractionalscalemanagerv1.h>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1

QW_USE_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

static WQuickFractionalScaleManagerV1 *FRACTIONAL_SCALE_MANAGER = nullptr;

class WQuickFractionalScaleManagerV1Private : public WObjectPrivate {
public:
    WQuickFractionalScaleManagerV1Private(WQuickFractionalScaleManagerV1 *qq)
        : WObjectPrivate(qq) {}
    ~WQuickFractionalScaleManagerV1Private() = default;

    W_DECLARE_PUBLIC(WQuickFractionalScaleManagerV1)

    QWFractionalScaleManagerV1 *manager = nullptr;
};

WQuickFractionalScaleManagerV1::WQuickFractionalScaleManagerV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickFractionalScaleManagerV1Private(this), nullptr)
{
    if (FRACTIONAL_SCALE_MANAGER) {
        qFatal("There are multiple instances of WQuickForeignToplevelManagerV1");
    }

    FRACTIONAL_SCALE_MANAGER = this;
}

void WQuickFractionalScaleManagerV1::create()
{
    W_D(WQuickFractionalScaleManagerV1);
    WQuickWaylandServerInterface::create();

    d->manager = QWFractionalScaleManagerV1::create(server()->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
}

WAYLIB_SERVER_END_NAMESPACE
