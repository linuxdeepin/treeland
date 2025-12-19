// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-server-treeland-output-manager-v1.h"

#include "outputmanagement.h"
#include "outputconfig.hpp"
#include "rootsurfacecontainer.h"
#include "helper.h"
#include "output.h"
#include <WOutput>

#define TREELAND_OUTPUT_MANAGER_V1_VERSION 2
#define TREELAND_OUTPUT_COLOR_CONTROL_V1_VERSION 1

WAYLIB_SERVER_USE_NAMESPACE

class ColorControlV1Private : public QtWaylandServer::treeland_output_color_control_v1
{
public:
    explicit ColorControlV1Private(ColorControlV1 *_q, wl_resource *resource, Output *output);

    ColorControlV1 *q;

    QPointer<Output> controlOutput;
    uint32_t pendingColorTemperature = 0;
    qreal pendingBrightness = -1;

    void send_brightness(qreal brightness);

protected:
    void treeland_output_color_control_v1_destroy_resource(Resource *resource) override;
    void treeland_output_color_control_v1_destroy(Resource *resource) override;

    void treeland_output_color_control_v1_set_color_temperature(Resource *resource, uint32_t temperature) override;
    void treeland_output_color_control_v1_set_brightness(Resource *resource, wl_fixed_t brightness) override;
    void treeland_output_color_control_v1_commit(Resource *resource) override;
};

ColorControlV1Private::ColorControlV1Private(ColorControlV1 *_q, wl_resource *resource, Output *output)
    : treeland_output_color_control_v1(resource)
    , q(_q)
    , controlOutput(output)
{
    auto *outputConfig = output->config();
    send_brightness(outputConfig->brightness());
    send_color_temperature(outputConfig->colorTemperature());

    QObject::connect(outputConfig,
            &OutputConfig::colorTemperatureChanged,
            q,
            [this, outputConfig] {
                send_color_temperature(outputConfig->colorTemperature());
            });
    QObject::connect(outputConfig,
            &OutputConfig::brightnessChanged,
            q,
            [this, outputConfig] {
                send_brightness(outputConfig->brightness());
            });
}

void ColorControlV1Private::treeland_output_color_control_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource);
    q->deleteLater();
}

void ColorControlV1Private::treeland_output_color_control_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorControlV1Private::treeland_output_color_control_v1_set_color_temperature(Resource *resource,
                                                                            uint32_t temperature)
{
    if (temperature < 1000 || temperature > 20000) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::treeland_output_color_control_v1::error_invalid_color_temperature,
                               "Color temperature must be between 1000K and 20000K");
        return;
    }
    pendingColorTemperature = temperature;
}

// NOTE: in treeland_output_color_control_v1 interface, brightness is in range [0.0, 100.0]
// but on the treeland side, output brightness is in range [0.0, 1.0], this is intentionally
// designed to allow finer control of brightness through wayland protocol,
// since the wl_fixed type have only 8 bits of precision.
void ColorControlV1Private::treeland_output_color_control_v1_set_brightness(Resource *resource,
                                                                     wl_fixed_t wl_brightness)
{
    qreal brightness = wl_fixed_to_double(wl_brightness);
    if (brightness < 0.0 || brightness > 100.0) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::treeland_output_color_control_v1::error_invalid_brightness,
                               "Brightness must be between 0.0 and 100.0");
        return;
    }
    pendingBrightness = brightness / 100.0;
}

void ColorControlV1Private::send_brightness(qreal brightness)
{
    // wl_fixed_from_double does not perform rounding
    // we add 1/512 (half of wl_fixed_t's precision) to maintain consistent mapping of brightness between protocol and treeland.
    treeland_output_color_control_v1::send_brightness(wl_fixed_from_double(brightness * 100.0 + 1.0 / 512));
}

void ColorControlV1Private::treeland_output_color_control_v1_commit(Resource *resource)
{
    Q_UNUSED(resource);
    if  (!controlOutput) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Output has been destroyed");
        return;
    }

    QPointer<ColorControlV1> guard(q);
    controlOutput->setOutputColor(pendingBrightness, pendingColorTemperature, [guard, this](bool success) {
        if (guard) {
            send_result(success ? 1 : 0);
        }
    });
    pendingBrightness = -1;
    pendingColorTemperature = 0;
}

ColorControlV1::ColorControlV1(wl_resource *resource, Output *output)
    : QObject(output)
    , d(new ColorControlV1Private(this, resource, output))
{
}

ColorControlV1::~ColorControlV1()
{
}

class OutputManagerV1Private : public QtWaylandServer::treeland_output_manager_v1
{
public:
    explicit OutputManagerV1Private(OutputManagerV1 *_q);
    wl_global *global() const;

    OutputManagerV1 *q;

protected:
    void treeland_output_manager_v1_bind_resource(Resource *resource) override;
    void treeland_output_manager_v1_destroy(Resource *resource) override;

    void treeland_output_manager_v1_set_primary_output(Resource *resource, const QString &output) override;
    void treeland_output_manager_v1_get_color_control(Resource *resource, uint32_t id, struct wl_resource *output) override;
};

OutputManagerV1Private::OutputManagerV1Private(OutputManagerV1 *_q)
    : q(_q)
{
}

wl_global *OutputManagerV1Private::global() const
{
    return m_global;
}

void OutputManagerV1Private::treeland_output_manager_v1_bind_resource(Resource *resource)
{
    auto *primaryOutput = Helper::instance()->rootSurfaceContainer()->primaryOutput();
    send_primary_output(resource->handle, primaryOutput ? primaryOutput->output()->name() : "");
}

void OutputManagerV1Private::treeland_output_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputManagerV1Private::treeland_output_manager_v1_set_primary_output(Resource *resource, const QString &output)
{
    Q_UNUSED(resource);
    auto *rootSurfaceContainer = Helper::instance()->rootSurfaceContainer();
    for (Output *o : std::as_const(rootSurfaceContainer->outputs())) {
        if (o->output()->name() == output) {
            rootSurfaceContainer->setPrimaryOutput(o);
            break;
        }
    }
}

void OutputManagerV1Private::treeland_output_manager_v1_get_color_control(Resource *resource,
                                                                          uint32_t id,
                                                                          struct wl_resource *output)
{
    auto *wlr_output = wlr_output_from_resource(output);
    if (!wlr_output) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Invalid output resource");
        return;
    }
    auto *o = Helper::instance()->getOutput(WOutput::fromHandle(qw_output::from(wlr_output)));
    if (!o) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Output not found");
        return;
    }

    auto *color_control_res = wl_resource_create(resource->client(),
                                                 QtWaylandServer::treeland_output_color_control_v1::interface(),
                                                 TREELAND_OUTPUT_COLOR_CONTROL_V1_VERSION,
                                                 id);
    if (!color_control_res) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto colorControl = new ColorControlV1(color_control_res, o);
    Q_UNUSED(colorControl);
}

OutputManagerV1::OutputManagerV1(QObject *parent)
    : QObject(parent)
    , d(new OutputManagerV1Private(this))
{
}

OutputManagerV1::~OutputManagerV1()
{
}

void OutputManagerV1::create(WServer *server)
{
    d->init(server->handle()->handle(), TREELAND_OUTPUT_MANAGER_V1_VERSION);
}

void OutputManagerV1::destroy([[maybe_unused]] WServer *server) { }

wl_global *OutputManagerV1::global() const
{
    return d->global();
}

QByteArrayView OutputManagerV1::interfaceName() const
{
    return d->interfaceName();
}

void OutputManagerV1::onPrimaryOutputChanged()
{
    auto *primaryOutput = Helper::instance()->rootSurfaceContainer()->primaryOutput();
    if (!primaryOutput)
        return;
    auto primaryOutputName = primaryOutput->output()->name();
    for (const auto &resource : d->resourceMap()) {
        d->send_primary_output(resource->handle, primaryOutputName);
    }
}
