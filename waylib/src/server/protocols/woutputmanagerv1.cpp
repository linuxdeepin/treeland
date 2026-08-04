// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputmanagerv1.h"
#include "woutputitem.h"
#include "private/wglobal_p.h"

#include <wlr/types/wlr_output_management_v1.h>

#include <QHash>

WAYLIB_SERVER_BEGIN_NAMESPACE

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
        return static_cast<wlr_output_manager_v1*>(q_func()->m_handle);
    }

    WScopedListener m_applyListener;
    WScopedListener m_testListener;
    wlr_output_manager_v1 *manager { nullptr };
    QPointer<WBackend> backend;
    QList<WOutputState> stateList;
    QHash<wlr_output_configuration_v1*, QList<WOutputState>> pendingStateLists;
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
        auto *woutput = WOutput::fromHandle(config_head->state.output);

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
        auto *wlr_output = state.output->nativeHandle();
        auto *configHead = wlr_output_configuration_head_v1_create(config, wlr_output);
        

        configHead->state.enabled = state.enabled;
        configHead->state.x = state.x;
        configHead->state.y = state.y;

        if (state.enabled) {
            configHead->state.mode = state.mode;
            configHead->state.scale = state.scale;
            configHead->state.transform = static_cast<wl_output_transform>(state.transform);
            configHead->state.adaptive_sync_enabled = state.adaptiveSyncEnabled;

            if (state.customModeSize.width() > 0 && state.customModeSize.height() > 0) {
                configHead->state.custom_mode.width = state.customModeSize.width();
                configHead->state.custom_mode.height = state.customModeSize.height();
                configHead->state.custom_mode.refresh = state.customModeRefresh;
            }
        }
    }

    wlr_output_manager_v1_set_configuration(d->manager, config);
}

void WOutputManagerV1::sendResult(wlr_output_configuration_v1 *config, bool ok)
{
    W_D(WOutputManagerV1);

    const QList<WOutputState> pendingStates = d->pendingStateLists.take(config);
    if (d->currentPendingConfig == config)
        d->currentPendingConfig = nullptr;

    if (ok) {
        wlr_output_configuration_v1_send_succeeded(config);
        d->stateList = pendingStates;
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
    const auto *wlr_output = output->handle();

    auto outputItem = WOutputItem::getOutputItem(output);

    WOutputState state {
        .output = output,
        .enabled = wlr_output->enabled,
        .mode = wlr_output->current_mode,
        .x = outputItem ? static_cast<int32_t>(outputItem->x()) : 0,
        .y = outputItem ? static_cast<int32_t>(outputItem->y()) : 0,
        .customModeSize = {  wlr_output->width,  wlr_output->height },
        .customModeRefresh =  wlr_output->refresh,
        .transform = static_cast<WOutput::Transform>(wlr_output->transform),
        .scale = wlr_output->scale,
        .adaptiveSyncEnabled = (wlr_output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED)
    };
    d->stateList.append(state);
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
    return static_cast<wlr_output_manager_v1*>(m_handle);
}

QByteArrayView WOutputManagerV1::interfaceName() const
{
    return "zwlr_output_head_v1";
}

void WOutputManagerV1::create(WServer *server)
{
    W_D(WOutputManagerV1);

    d->manager = wlr_output_manager_v1_create(server->handle());
    d->m_testListener.connect(&d->manager->events.test, [d](wl_listener *, void *data) {
        d->outputMgrApplyOrTest(static_cast<wlr_output_configuration_v1*>(data), true);
    });
    d->m_applyListener.connect(&d->manager->events.apply, [d](wl_listener *, void *data) {
        d->outputMgrApplyOrTest(static_cast<wlr_output_configuration_v1*>(data), false);
    });
}

wl_global *WOutputManagerV1::global() const
{
    W_D(const WOutputManagerV1);

    if (m_handle)
        return d->handle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
