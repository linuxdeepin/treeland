// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputmanagement.h"
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
    return treeland_output_manager_v1_interface.name;
}

void OutputManagerV1::onColorControlCreated(treeland_output_color_control_v1 *control)
{
    auto *output = Helper::instance()->getOutput(WOutput::fromHandle(control->output));

    control->blockSignals(true);

    connect(control,
            &treeland_output_color_control_v1::requestSetColorTemperature,
            output,
            &Output::setColorTemperature);
    connect(control,
            &treeland_output_color_control_v1::requestSetBrightness,
            output,
            [output](uint32_t brightness) {
                output->setBrightness(static_cast<qreal>(brightness)/100.0);
            });

    connect(output,
            &Output::colorTemperatureChanged,
            control,
            [output, control] {
                control->sendColorTemperature(output->colorTemperature());
            });
    connect(output,
            &Output::brightnessChanged,
            control,
            [output, control] {
                control->sendBrightness(static_cast<uint32_t>(output->brightness() * 100));
            });
    
    control->sendBrightness(static_cast<uint32_t>(output->brightness() * 100));
    control->sendColorTemperature(output->colorTemperature());

    control->blockSignals(false);
}
