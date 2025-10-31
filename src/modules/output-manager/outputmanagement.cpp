// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputmanagement.h"
#include "outputconfig.hpp"
#include "helper.h"
#include "output.h"
#include <WOutput>

WAYLIB_SERVER_USE_NAMESPACE

#include "modules/output-manager/impl/output_manager_impl.h"

OutputManagerV1::OutputManagerV1(QObject *parent)
    : QObject(parent)
{
}

void OutputManagerV1::sendPrimaryOutput(const char *name)
{
    m_handle->set_primary_output(name);
}

void OutputManagerV1::create(WServer *server)
{
    m_handle = treeland_output_manager_v1::create(server->handle());

    connect(m_handle,
            &treeland_output_manager_v1::requestSetPrimaryOutput,
            this,
            &OutputManagerV1::requestSetPrimaryOutput);
    
    connect(m_handle,
            &treeland_output_manager_v1::colorControlCreated,
            this,
            &OutputManagerV1::onColorControlCreated);
}

void OutputManagerV1::destroy([[maybe_unused]] WServer *server) { }

wl_global *OutputManagerV1::global() const
{
    return m_handle->global;
}

QByteArrayView OutputManagerV1::interfaceName() const
{
    const static QByteArray interfaceName(treeland_output_manager_v1_interface.name);
    return interfaceName;
}

void OutputManagerV1::onColorControlCreated(treeland_output_color_control_v1 *control)
{
    auto *outputConfig = Helper::instance()->getOutput(WOutput::fromHandle(control->output))->config();

    connect(control,
            &treeland_output_color_control_v1::requestSetColorTemperature,
            outputConfig,
            &OutputConfig::setColorTemperature);
    connect(control,
            &treeland_output_color_control_v1::requestSetBrightness,
            outputConfig,
            &OutputConfig::setBrightness);

    connect(outputConfig,
            &OutputConfig::colorTemperatureChanged,
            control,
            [outputConfig, control] {
                control->sendColorTemperature(outputConfig->colorTemperature());
            });
    connect(outputConfig,
            &OutputConfig::brightnessChanged,
            control,
            [outputConfig, control] {
                control->sendBrightness(outputConfig->brightness());
            });

    control->sendBrightness(outputConfig->brightness());
    control->sendColorTemperature(outputConfig->colorTemperature());
};
