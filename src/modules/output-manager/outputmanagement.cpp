// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-server-treeland-output-manager-unstable-v2.h"

#include "outputmanagement.h"
#include "outputconfig.hpp"
#include "rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "output.h"

#include <WOutput>

#include <optional>
#include <wayland-server-core.h>

WAYLIB_SERVER_USE_NAMESPACE

class PictureControlV2Private : public QtWaylandServer::treeland_output_picture_control_v2
{
public:
    explicit PictureControlV2Private(PictureControlV2 *_q, wl_resource *resource, const QString &uuid);

    PictureControlV2 *q;

    // v2: invalid_output is terminal. Once it has been delivered on this object
    // (at creation time or in response to a commit) the object is permanently
    // inert: only destroy stays valid, no further result, color_temperature or
    // brightness event is emitted, and it never recovers even if an output
    // with this uuid is connected later. The client must destroy it and call
    // get_picture_control again to get a usable object.
    bool inert = false;

    QString uuid;
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

private:
    void resolveOutput();
    void attachOutput(Output *output);
    void detachOutput();
    void becomeInert();

    QList<QMetaObject::Connection> modelConnections;
    QList<QMetaObject::Connection> configConnections;
};

PictureControlV2Private::PictureControlV2Private(PictureControlV2 *_q, wl_resource *resource, const QString &_uuid)
    : treeland_output_picture_control_v2(resource)
    , q(_q)
    , uuid(_uuid)
{
    // The uuid must identify a live enabled output at creation time. Otherwise
    // the object is still created, but v2 requires an immediate result
    // (invalid_output) instead of the initial values, and the object is inert
    // from the start: no output model tracking, no recovery.
    auto *output = WaylandServer::instance()->getOutputByUuid(uuid);
    if (!output || !output->output()->isEnabled()) {
        send_result(commit_result_invalid_output);
        becomeInert();
        return;
    }

    // Commits and value events follow the output that currently carries this
    // uuid, so re-resolve whenever the output model changes.
    auto *model = WaylandServer::instance()->rootSurfaceContainer()->outputModel();
    modelConnections = {
        QObject::connect(model, &QAbstractItemModel::rowsInserted, q, [this] {
            resolveOutput();
        }),
        QObject::connect(model, &QAbstractItemModel::rowsRemoved, q, [this] {
            resolveOutput();
        }),
    };

    attachOutput(output);
}

void PictureControlV2Private::resolveOutput()
{
    // The lookup itself (and why it must go through the output model) lives
    // in WaylandServer::getOutputByUuid().
    attachOutput(WaylandServer::instance()->getOutputByUuid(uuid));
}

void PictureControlV2Private::detachOutput()
{
    for (const auto &connection : std::as_const(configConnections))
        QObject::disconnect(connection);
    configConnections.clear();
    controlOutput = nullptr;
}

void PictureControlV2Private::becomeInert()
{
    if (inert)
        return;

    inert = true;

    // An inert object stops following the output model: it must not pick up an
    // output that appears later with the same uuid.
    for (const auto &connection : std::as_const(modelConnections))
        QObject::disconnect(connection);
    modelConnections.clear();

    detachOutput();
}

void PictureControlV2Private::attachOutput(Output *output)
{
    if (inert || controlOutput == output)
        return;

    detachOutput();

    controlOutput = output;
    if (!controlOutput)
        return;

    auto *outputConfig = controlOutput->config();
    send_brightness(outputConfig->brightness());
    send_color_temperature(outputConfig->colorTemperature());

    configConnections = {
        QObject::connect(outputConfig,
                         &OutputConfig::colorTemperatureChanged,
                         q,
                         [this, outputConfig] {
                             send_color_temperature(outputConfig->colorTemperature());
                         }),
        QObject::connect(outputConfig,
                         &OutputConfig::brightnessChanged,
                         q,
                         [this, outputConfig] {
                             send_brightness(outputConfig->brightness());
                         }),
    };
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
    // v2: range validation is a fatal protocol error on the request itself and
    // stays valid on an inert object; only the pending state is dropped, and no
    // event is ever emitted for it.
    if (inert)
        return;
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
    // See set_color_temperature: keep the fatal range check, drop the value.
    if (inert)
        return;
    pendingBrightness = brightness;
}

void PictureControlV2Private::send_brightness(qreal brightness)
{
    // brightness is internal [0.0, 1.0]; protocol range is [0.0, 100.0] (wl_fixed_t).
    // wl_fixed_from_double() rounds, so full brightness maps to exactly 100.0
    // and stays inside the range the client may send back.
    treeland_output_picture_control_v2::send_brightness(wl_fixed_from_double(brightness * 100.0));
}

void PictureControlV2Private::commit(Resource *resource)
{
    Q_UNUSED(resource);
    // v2: an inert object only accepts destroy; commit produces no event at all.
    if (inert)
        return;

    // v2: range validation happens on the set_* requests with a fatal protocol
    // error, so a commit only fails for output-related reasons
    // (invalid_output, unsupported, failed).
    if (!controlOutput || !controlOutput->output()->isEnabled()) {
        pendingColorTemperature.reset();
        pendingBrightness.reset();
        // invalid_output is the last event this object ever sends: it becomes
        // permanently inert right after, whether the output was missing from
        // the start or was unplugged/disabled between two commits.
        send_result(commit_result_invalid_output);
        becomeInert();
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

PictureControlV2::PictureControlV2(wl_resource *resource, const QString &uuid, QObject *parent)
    : QObject(parent)
    , d(new PictureControlV2Private(this, resource, uuid))
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

    void set_primary_output(Resource *resource, const QString &uuid) override;
    void get_picture_control(Resource *resource, uint32_t id, const QString &uuid) override;
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
    // v2: emit primary_output immediately after bind, carrying the uuid of the
    // current primary output (empty when none is available).
    auto *primaryOutput = WaylandServer::instance()->rootSurfaceContainer()->primaryOutput();
    send_primary_output(resource->handle, primaryOutput ? primaryOutput->uuid() : QString());
}

void OutputManagerV2Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputManagerV2Private::set_primary_output(Resource *resource, const QString &uuid)
{
    auto *o = WaylandServer::instance()->getOutputByUuid(uuid);
    // v2: an empty or unknown uuid, and a disabled output, are rejected via
    // primary_output_failed instead of a protocol error.
    if (!o || !o->output()->isEnabled()) {
        send_primary_output_failed(resource->handle, primary_output_failed_reason_invalid_output);
        return;
    }

    auto *container = WaylandServer::instance()->rootSurfaceContainer();
    // setPrimaryOutput() is a no-op when the output is already primary, but v2
    // requires a primary_output event to confirm every set_primary_output
    // request, so confirm same-value requests explicitly.
    const bool changed = container->primaryOutput() != o;
    container->setPrimaryOutput(o, true);
    if (!changed)
        q->onPrimaryOutputChanged();
}

void OutputManagerV2Private::get_picture_control(Resource *resource,
                                                  uint32_t id,
                                                  const QString &uuid)
{
    // The object is always created, even for an empty or unknown uuid. When the
    // uuid does not identify a live enabled output, PictureControlV2 reports
    // result(invalid_output) immediately and becomes permanently inert; the
    // client must destroy it and call this request again.
    auto *picture_control_res = wl_resource_create(resource->client(),
                                                   QtWaylandServer::treeland_output_picture_control_v2::interface(),
                                                   OutputManagerV2::PictureControlInterfaceVersion,
                                                   id);
    if (!picture_control_res) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    // The PictureControlV2 object is owned by its wl_resource: destroy_resource()
    // schedules q->deleteLater(). It must not be deleted here or leaked.
    auto pictureControl = new PictureControlV2(picture_control_res, uuid, q);
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
    auto *primaryOutput = WaylandServer::instance()->rootSurfaceContainer()->primaryOutput();
    for (const auto &resource : d->resourceMap()) {
        d->send_primary_output(resource->handle,
                               primaryOutput ? primaryOutput->uuid() : QString());
    }
}
