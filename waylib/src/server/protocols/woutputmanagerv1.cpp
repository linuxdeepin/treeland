// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputmanagerv1.h"
#include "wscoplistener.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

#include <QHash>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

WOutputState outputState(WOutput *output)
{
    const auto *wlrOutput = output->handle();
    const QPoint position = output->position();

    return {
        .output = output,
        .enabled = wlrOutput->enabled,
        .mode = wlrOutput->current_mode,
        .x = position.x(),
        .y = position.y(),
        .customModeSize = { wlrOutput->width, wlrOutput->height },
        .customModeRefresh = wlrOutput->refresh,
        .transform = static_cast<WOutput::Transform>(wlrOutput->transform),
        .scale = wlrOutput->scale,
        .adaptiveSyncEnabled =
            wlrOutput->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED
    };
}

}

class Q_DECL_HIDDEN WOutputManagerV1Private : public WObjectPrivate
{
public:
    WOutputManagerV1Private(WOutputManagerV1 *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WOutputManagerV1)

    void outputMgrApplyOrTest(wlr_output_configuration_v1 *config, int test);
    inline wlr_output_manager_v1 *handle() const {
        return reinterpret_cast<wlr_output_manager_v1*>(q_func()->m_handle);
    }

    wlr_output_manager_v1 *manager { nullptr };
    QPointer<WBackend> backend;
    QList<WOutputState> stateList;
    QHash<wlr_output_configuration_v1 *, QList<WOutputState>> pendingStateLists;
    wlr_output_configuration_v1 *currentPendingConfig { nullptr };
};

WOutputManagerV1::WOutputManagerV1()
    : WObject(*new WOutputManagerV1Private(this))
{

}

void WOutputManagerV1Private::outputMgrApplyOrTest(wlr_output_configuration_v1 *config, int onlyTest)
{
    W_Q(WOutputManagerV1);
    wlr_output_configuration_head_v1 *config_head;

    QList<WOutputState> pendingStates;

    wl_list_for_each(config_head, &config->heads, link) {
        auto *output = config_head->state.output;
        auto *woutput = WOutput::fromHandle(output);

        const auto &state = config_head->state;

        pendingStates.append(WOutputState {
            .output = woutput,
            .enabled = state.enabled,
            .mode = state.mode,
            .x = state.x,
            .y = state.y,
            .customModeSize = { state.custom_mode.width, state.custom_mode.height },
            .customModeRefresh = state.custom_mode.refresh,
            .transform = static_cast<WOutput::Transform>(state.transform),
            .scale = state.scale,
            .adaptiveSyncEnabled = state.adaptive_sync_enabled
        });
    }

    currentPendingConfig = config;
    pendingStateLists.insert(config, std::move(pendingStates));
    Q_EMIT q->requestTestOrApply(config, onlyTest);
}

QList<WOutputState> WOutputManagerV1::stateListPending(wlr_output_configuration_v1 *config) const
{
    W_D(const WOutputManagerV1);
    return d->pendingStateLists.value(config ? config : d->currentPendingConfig);
}

void WOutputManagerV1::updateConfig()
{
    W_D(WOutputManagerV1);

    auto *config = wlr_output_configuration_v1_create();
    for (const WOutputState &state : std::as_const(d->stateList)) {
        auto *wlr_output = state.output->handle();
        auto *configHead = wlr_output_configuration_head_v1_create(config, wlr_output);
        auto *handle = &configHead->state;

        handle->enabled = state.enabled;
        handle->x = state.x;
        handle->y = state.y;

        if (state.enabled) {
            handle->mode = state.mode;
            handle->scale = state.scale;
            handle->transform = static_cast<wl_output_transform>(state.transform);
            handle->adaptive_sync_enabled = state.adaptiveSyncEnabled;

            if (state.customModeSize.width() > 0 && state.customModeSize.height() > 0) {
                handle->custom_mode.width = state.customModeSize.width();
                handle->custom_mode.height = state.customModeSize.height();
                handle->custom_mode.refresh = state.customModeRefresh;
            }
        }
    }

    wlr_output_manager_v1_set_configuration(d->manager, config);
}

void WOutputManagerV1::sendResult(wlr_output_configuration_v1 *config, bool ok)
{
    W_D(WOutputManagerV1);
    sendResult(config, ok, d->pendingStateLists.value(config));
}

void WOutputManagerV1::sendResult(wlr_output_configuration_v1 *config,
                                  bool ok,
                                  const QList<WOutputState> &appliedStates)
{
    W_D(WOutputManagerV1);

    d->pendingStateLists.remove(config);
    if (d->currentPendingConfig == config)
        d->currentPendingConfig = nullptr;

    if (ok) {
        wlr_output_configuration_v1_send_succeeded(config);
        d->stateList = appliedStates;
    } else {
        wlr_output_configuration_v1_send_failed(config);
    }

    wlr_output_configuration_v1_destroy(config);

    // Schedule updateConfig through the event loop to avoid recursion
    QMetaObject::invokeMethod(this, &WOutputManagerV1::updateConfig, Qt::QueuedConnection);
}

void WOutputManagerV1::newOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    d->stateList.append(outputState(output));
    connect(output, &WOutput::enabledChanged,
            this, &WOutputManagerV1::syncOutputStates);
    connect(output, &WOutput::positionChanged,
            this, &WOutputManagerV1::syncOutputStates);
    connect(output, &WOutput::modeChanged,
            this, &WOutputManagerV1::syncOutputStates);
    connect(output, &WOutput::orientationChanged,
            this, &WOutputManagerV1::syncOutputStates);
    connect(output, &WOutput::scaleChanged,
            this, &WOutputManagerV1::syncOutputStates);
    connect(output, &WOutput::adaptiveSyncEnabledChanged,
            this, &WOutputManagerV1::syncOutputStates);
    updateConfig();
}

void WOutputManagerV1::syncOutputStates()
{
    W_D(WOutputManagerV1);
    for (auto &state : d->stateList)
        state = outputState(state.output);

    updateConfig();
}

void WOutputManagerV1::removeOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    d->stateList.removeIf([output](const WOutputState &s) {
        return s.output == output;
    });

    updateConfig();
}

wlr_output_manager_v1 *WOutputManagerV1::handle() const
{
    return reinterpret_cast<wlr_output_manager_v1*>(m_handle);
}

QByteArrayView WOutputManagerV1::interfaceName() const
{
    return "zwlr_output_manager_v1";
}

void WOutputManagerV1::create(WServer *server)
{
    W_D(WOutputManagerV1);

    d->manager = wlr_output_manager_v1_create(server->handle());
    m_handle = d->manager;
    listeners()->add(&d->manager->events.test, this,
        [d] (wlr_output_configuration_v1 *config) {
        d->outputMgrApplyOrTest(config, true);
    });

    listeners()->add(&d->manager->events.apply, this,
        [d] (wlr_output_configuration_v1 *config) {
        d->outputMgrApplyOrTest(config, false);
    });
}

void WOutputManagerV1::destroy([[maybe_unused]] WServer *server)
{
    // apply/test listeners are detached by WServer::stop()/detach() via
    // WObject::teardown() before this runs (wlr_output_manager_v1 asserts
    // those lists are empty when the display destroys the manager).
    m_handle = nullptr;
}

wl_global *WOutputManagerV1::global() const
{
    W_D(const WOutputManagerV1);

    if (m_handle)
        return d->handle()->global;
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
