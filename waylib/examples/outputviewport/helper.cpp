// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <qwoutput.h>

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frmae. Because QWOutput::frame signal
    // maybe emit before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this ouptut on render.
    if (!qwoutput->property("_Enabled").toBool()) {
        qwoutput->setProperty("_Enabled", true);

        if (!qwoutput->handle()->current_mode) {
            auto mode = qwoutput->preferredMode();
            if (mode)
                output->setMode(mode);
        }
        output->enable(true);
        bool ok = output->commit();
        Q_ASSERT(ok);
    }
}
