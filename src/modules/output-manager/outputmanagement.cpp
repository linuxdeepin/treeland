// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-server-treeland-output-manager-unstable-v2.h"

#include "outputmanagement.h"
#include "outputconfig.hpp"
#include "rootsurfacecontainer.h"
#include "helper.h"
#include "output.h"

#include <WOutput>

#include <optional>
#include <wayland-server-core.h>

WAYLIB_SERVER_USE_NAMESPACE

// Returns the wl_output resource belonging to @p client from @p wlrOutput's
// resource list, or null when the client has not bound that output.
static wl_resource *outputResourceForClient(struct wlr_output *wlrOutput,
                                            wl_client *client)
{
    if (!wlrOutput || !client)
        return nullptr;

    wl_resource *resource = nullptr;
    wl_resource_for_each(resource, &wlrOutput->resources)
    {
        if (wl_resource_get_client(resource) == client)
            return resource;
    }
    return nullptr;
}

class PictureControlV2Private : public QtWaylandServer::treeland_output_picture_control_v2
{
public:
    explicit PictureControlV2Private(PictureControlV2 *_q, wl_resource *resource, Output *output);

    PictureControlV2 *q;

    QPointer<Output> controlOutput;

    // Pending protocol values accumulated since the last commit; nullopt means
    // "not set". Color temperature valid range [1000, 20000], brightness
    // [0.0, 100.0]. Out-of-range set_* values are rejected with a fatal
    // protocol error and never reach the pending state.
    std::optional<uint32_t> pendingColorTemperature;
    std::optional<qreal> pendingBrightness;

    void send_brightness(qreal brightness);

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;

    void set_color_temperature(Resource *resource, uint32_t temperature) override;
    void set_brightness(Resource *resource, wl_fixed_t brightness) override;
    void commit(Resource *resource) override;
};

PictureControlV2Private::PictureControlV2Private(PictureControlV2 *_q, wl_resource *resource, Output *output)
    : treeland_output_picture_control_v2(resource)
    , q(_q)
    , controlOutput(output)
{
    if (!controlOutput)
        return;

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

void PictureControlV2Private::destroy_resource(Resource *resource)
{
    Q_UNUSED(resource);
    q->deleteLater();
}

void PictureControlV2Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PictureControlV2Private::set_color_temperature(Resource *resource,
                                                     uint32_t temperature)
{
    if (temperature < 1000 || temperature > 20000) {
        wl_resource_post_error(resource->handle,
                               error_invalid_color_temperature,
                               "Color temperature must be between 1000K and 20000K");
        return;
    }
    pendingColorTemperature = temperature;
}

void PictureControlV2Private::set_brightness(Resource *resource,
                                              wl_fixed_t wl_brightness)
{
    const qreal brightness = wl_fixed_to_double(wl_brightness);
    if (brightness < 0.0 || brightness > 100.0) {
        wl_resource_post_error(resource->handle,
                               error_invalid_brightness,
                               "Brightness must be between 0.0 and 100.0");
        return;
    }
    pendingBrightness = brightness;
}

void PictureControlV2Private::send_brightness(qreal brightness)
{
    // brightness is internal [0.0, 1.0]; protocol range is [0.0, 100.0] (wl_fixed_t).
    treeland_output_picture_control_v2::send_brightness(wl_fixed_from_double(brightness * 100.0));
}

void PictureControlV2Private::commit(Resource *resource)
{
    Q_UNUSED(resource);
    // v2: range validation happens on the set_* requests with a fatal protocol
    // error, so a commit only fails for output-related reasons
    // (invalid_output, unsupported, failed).
    if (!controlOutput) {
        send_result(commit_result_invalid_output);
        pendingColorTemperature.reset();
        pendingBrightness.reset();
        return;
    }

    // Nothing to commit — report success immediately.
    if (!pendingColorTemperature && !pendingBrightness) {
        send_result(commit_result_success);
        return;
    }

    // Convert protocol values to treeland internal values.
    // Brightness: protocol [0.0, 100.0] → treeland [0.0, 1.0].
    // -1 / 0 sentinel means "not requested" for setOutputColor.
    qreal treelandBrightness = pendingBrightness ? *pendingBrightness / 100.0 : -1;
    uint32_t treelandColorTemp = pendingColorTemperature ? *pendingColorTemperature : 0;

    QPointer<PictureControlV2> guard(q);
    controlOutput->setOutputColor(treelandBrightness, treelandColorTemp, [guard, this](Output::CommitColorResult result) {
        if (!guard)
            return;
        switch (result) {
        case Output::CommitColorResult::Success:
            send_result(commit_result_success);
            break;
        case Output::CommitColorResult::Unsupported:
            send_result(commit_result_unsupported);
            break;
        case Output::CommitColorResult::Failed:
            send_result(commit_result_failed);
            break;
        }
    });
    pendingColorTemperature.reset();
    pendingBrightness.reset();
}

PictureControlV2::PictureControlV2(wl_resource *resource, Output *output)
    : QObject(output)
    , d(new PictureControlV2Private(this, resource, output))
{
}

PictureControlV2::~PictureControlV2()
{
}

class OutputManagerV2Private : public QtWaylandServer::treeland_output_manager_v2
{
public:
    explicit OutputManagerV2Private(OutputManagerV2 *_q);
    wl_global *global() const;

    OutputManagerV2 *q;

protected:
    void bind_resource(Resource *resource) override;
    void destroy(Resource *resource) override;

    void set_primary_output(Resource *resource, struct ::wl_resource *output) override;
    void get_picture_control(Resource *resource, uint32_t id, struct ::wl_resource *output) override;
};

OutputManagerV2Private::OutputManagerV2Private(OutputManagerV2 *_q)
    : QtWaylandServer::treeland_output_manager_v2()
    , q(_q)
{
}

wl_global *OutputManagerV2Private::global() const
{
    return m_global;
}

void OutputManagerV2Private::bind_resource(Resource *resource)
{
    // v2: emit primary_output immediately after bind, carrying the wl_output
    // object (or null when no output is available).
    auto *primaryOutput = Helper::instance()->rootSurfaceContainer()->primaryOutput();
    auto *client = wl_resource_get_client(resource->handle);
    wl_resource *outputResource = nullptr;
    if (primaryOutput)
        outputResource = outputResourceForClient(primaryOutput->output()->handle(), client);
    send_primary_output(resource->handle, outputResource);
}

void OutputManagerV2Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputManagerV2Private::set_primary_output(Resource *resource, struct ::wl_resource *output)
{
    // libwayland delivers a null object id (id 0) as a NULL resource without
    // rejecting it, and wlr_output_from_resource() dereferences the resource,
    // so guard before touching wlroots. Per spec a null output is rejected via
    // primary_output_failed(invalid_output), not a core protocol error.
    if (!output) {
        send_primary_output_failed(resource->handle, primary_output_failed_reason_invalid_output);
        return;
    }
    auto *wlr_output = wlr_output_from_resource(output);
    if (!wlr_output) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Invalid output resource");
        return;
    }
    auto *o = Helper::instance()->getOutput(WOutput::fromHandle(wlr_output));
    // v2: reject disabled or unavailable outputs via primary_output_failed
    // instead of silently ignoring the request.
    if (!o || !o->output()->isEnabled()) {
        send_primary_output_failed(resource->handle, primary_output_failed_reason_invalid_output);
        return;
    }
    Helper::instance()->rootSurfaceContainer()->setPrimaryOutput(o, true);
}

void OutputManagerV2Private::get_picture_control(Resource *resource,
                                                  uint32_t id,
                                                  struct ::wl_resource *output)
{
    // do not raise a fatal display error for an invalid output every subsequent
    // commit then reports commit_result_invalid_output
    Output *o = nullptr;
    if (output) {
        auto *wlr_output = wlr_output_from_resource(output);
        if (wlr_output)
            o = Helper::instance()->getOutput(WOutput::fromHandle(wlr_output));
    }

    auto *picture_control_res = wl_resource_create(resource->client(),
                                                   QtWaylandServer::treeland_output_picture_control_v2::interface(),
                                                   OutputManagerV2::PictureControlInterfaceVersion,
                                                   id);
    if (!picture_control_res) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto pictureControl = new PictureControlV2(picture_control_res, o);
    Q_UNUSED(pictureControl);
}

OutputManagerV2::OutputManagerV2(QObject *parent)
    : QObject(parent)
    , d(new OutputManagerV2Private(this))
{
}

OutputManagerV2::~OutputManagerV2()
{
}

void OutputManagerV2::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void OutputManagerV2::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *OutputManagerV2::global() const
{
    return d->global();
}

QByteArrayView OutputManagerV2::interfaceName() const
{
    return d->interfaceName();
}

void OutputManagerV2::onPrimaryOutputChanged()
{
    auto *primaryOutput = Helper::instance()->rootSurfaceContainer()->primaryOutput();
    for (const auto &resource : d->resourceMap()) {
        auto *client = wl_resource_get_client(resource->handle);
        wl_resource *outputResource = nullptr;
        if (primaryOutput)
            outputResource = outputResourceForClient(primaryOutput->output()->handle(), client);
        d->send_primary_output(resource->handle, outputResource);
    }
}
