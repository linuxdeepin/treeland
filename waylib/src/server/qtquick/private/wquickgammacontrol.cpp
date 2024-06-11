// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickgammacontrol_p.h"
#include "woutput.h"
#include "private/wglobal_p.h"

#include <qwgammacontorlv1.h>
#include <qwoutput.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWGammaControlManagerV1;

class WQuickGammaControlManagerPrivate : public WObjectPrivate
{
public:
    WQuickGammaControlManagerPrivate(WQuickGammaControlManager *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickGammaControlManager)

    QWGammaControlManagerV1 *manager = nullptr;
};

WQuickGammaControlManager* WQuickGammaControlManager::GAMMA_CONTROL_MANAGER = nullptr;

WQuickGammaControlManager::WQuickGammaControlManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickGammaControlManagerPrivate(this), nullptr)
{
    if (GAMMA_CONTROL_MANAGER) {
        qFatal("There are multiple instances of WQuickGammaControlManager");
    }

    GAMMA_CONTROL_MANAGER = this;
}

WServerInterface *WQuickGammaControlManager::create()
{
    W_D(WQuickGammaControlManager);

    d->manager = QWGammaControlManagerV1::create(server()->handle());
    connect(d->manager, &QWGammaControlManagerV1::gammaChanged, this,
            [this] (wlr_gamma_control_manager_v1_set_gamma_event *event) {
                auto *qwOutput = QW_NAMESPACE::QWOutput::from(event->output);
                auto *wOutput = WOutput::fromHandle(qwOutput);
                size_t ramp_size = 0;
                uint16_t *r = nullptr, *g = nullptr, *b = nullptr;
                wlr_gamma_control_v1 *gamma_control = event->control;
                if (gamma_control) {
                    ramp_size = gamma_control->ramp_size;
                    r = gamma_control->table;
                    g = gamma_control->table + gamma_control->ramp_size;
                    b = gamma_control->table + 2 * gamma_control->ramp_size;
                }
                Q_EMIT gammaChanged(wOutput, gamma_control, ramp_size, r, g, b);
    });

    return new WServerInterface(d->manager, d->manager->handle()->global);
}

wlr_gamma_control_v1 *WQuickGammaControlManager::getControl(wlr_output *output)
{
    W_D(WQuickGammaControlManager);
    return wlr_gamma_control_manager_v1_get_control(d->manager->handle(), output);
}

void WQuickGammaControlManager::sendFailedAndDestroy(wlr_gamma_control_v1 *control)
{
    wlr_gamma_control_v1_send_failed_and_destroy(control);
}

WAYLIB_SERVER_END_NAMESPACE
