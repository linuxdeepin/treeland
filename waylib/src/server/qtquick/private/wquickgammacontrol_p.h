// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <QQmlEngine>

extern "C" {
#include <wlr/types/wlr_gamma_control_v1.h>
}

struct wlr_output;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
class WOutput;

class WQuickGammaControlManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickGammaControlManager: public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickGammaControlManager)
    QML_NAMED_ELEMENT(GammaControlManager)
public:
    explicit WQuickGammaControlManager(QObject *parent = nullptr);
    wlr_gamma_control_v1 *getControl(wlr_output *output);
    Q_INVOKABLE void sendFailedAndDestroy(wlr_gamma_control_v1 *control);

    static WQuickGammaControlManager *GAMMA_CONTROL_MANAGER;

Q_SIGNALS:
    void gammaChanged(WOutput *output, wlr_gamma_control_v1 *gamma_control,
                      size_t ramp_size, uint16_t *r, uint16_t *g, uint16_t *b);

private:
    WServerInterface *create() override;
};


WAYLIB_SERVER_END_NAMESPACE
