// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputmanagerv1.h"
#include "woutputitem.h"
#include "private/wglobal_p.h"

extern "C" {
#include <wlr/types/wlr_output_management_v1.h>
}

#include <QHash>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputManagerV1Private : public WObjectPrivate
{
public:
    explicit WOutputManagerV1Private(WOutputManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    void outputMgrApplyOrTest(wlr_output_configuration_v1 *config, bool onlyTest)
    {
        W_Q(WOutputManagerV1);
        QList<WOutputState> pendingStates;
        wlr_output_configuration_head_v1 *configHead;
        wl_list_for_each(configHead, &config->heads, link) {
            const auto &state = configHead->state;
            pendingStates.append(WOutputState {
                .output = WOutput::fromHandle(state.output),
                .enabled = state.enabled,
                .mode = state.mode,
                .x = state.x,
                .y = state.y,
                .customModeSize = { state.custom_mode.width, state.custom_mode.height },
                .customModeRefresh = state.custom_mode.refresh,
                .transform = static_cast<WOutput::Transform>(state.transform),
                .scale = state.scale,
                .adaptiveSyncEnabled = state.adaptive_sync_enabled,
            });
        }

        currentPendingConfig = config;
        pendingStateLists.insert(config, std::move(pendingStates));
        Q_EMIT q->requestTestOrApply(config, onlyTest);
    }

    void releasePendingConfigurations()
    {
        const auto configurations = pendingStateLists.keys();
        pendingStateLists.clear();
        currentPendingConfig = nullptr;
        for (auto *config : configurations) {
            wlr_output_configuration_v1_send_failed(config);
            wlr_output_configuration_v1_destroy(config);
        }
    }

    W_DECLARE_PUBLIC(WOutputManagerV1)

    QList<WOutputState> stateList;
    QHash<wlr_output_configuration_v1 *, QList<WOutputState>> pendingStateLists;
    wlr_output_configuration_v1 *currentPendingConfig = nullptr;
    WNativeListener testListener;
    WNativeListener applyListener;
};

WOutputManagerV1::WOutputManagerV1()
    : WObject(*new WOutputManagerV1Private(this))
{
}

QList<WOutputState> WOutputManagerV1::stateListPending(wlr_output_configuration_v1 *config) const
{
    W_DC(WOutputManagerV1);
    return d->pendingStateLists.value(config ? config : d->currentPendingConfig);
}

void WOutputManagerV1::updateConfig()
{
    W_D(WOutputManagerV1);
    if (!handle())
        return;

    auto *config = wlr_output_configuration_v1_create();
    Q_ASSERT(config);
    for (const WOutputState &state : std::as_const(d->stateList)) {
        auto *configHead = wlr_output_configuration_head_v1_create(config, state.output->handle());
        Q_ASSERT(configHead);
        auto &headState = configHead->state;
        headState.enabled = state.enabled;
        headState.x = state.x;
        headState.y = state.y;

        if (state.enabled) {
            headState.mode = state.mode;
            headState.scale = state.scale;
            headState.transform = static_cast<wl_output_transform>(state.transform);
            headState.adaptive_sync_enabled = state.adaptiveSyncEnabled;
            if (state.customModeSize.width() > 0 && state.customModeSize.height() > 0) {
                headState.custom_mode.width = state.customModeSize.width();
                headState.custom_mode.height = state.customModeSize.height();
                headState.custom_mode.refresh = state.customModeRefresh;
            }
        }
    }

    wlr_output_manager_v1_set_configuration(handle(), config);
}

void WOutputManagerV1::sendResult(wlr_output_configuration_v1 *config, bool ok)
{
    W_D(WOutputManagerV1);
    sendResult(config, ok, d->pendingStateLists.value(config));
}

void WOutputManagerV1::sendResult(wlr_output_configuration_v1 *config, bool ok,
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

    QMetaObject::invokeMethod(this, &WOutputManagerV1::updateConfig, Qt::QueuedConnection);
}

void WOutputManagerV1::newOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    const auto *nativeOutput = output->handle();
    auto *outputItem = WOutputItem::getOutputItem(output);

    d->stateList.append(WOutputState {
        .output = output,
        .enabled = nativeOutput->enabled,
        .mode = nativeOutput->current_mode,
        .x = outputItem ? static_cast<int32_t>(outputItem->x()) : 0,
        .y = outputItem ? static_cast<int32_t>(outputItem->y()) : 0,
        .customModeSize = { nativeOutput->width, nativeOutput->height },
        .customModeRefresh = nativeOutput->refresh,
        .transform = static_cast<WOutput::Transform>(nativeOutput->transform),
        .scale = nativeOutput->scale,
        .adaptiveSyncEnabled = nativeOutput->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED,
    });
    updateConfig();
}

void WOutputManagerV1::removeOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    d->stateList.removeIf([output](const WOutputState &state) {
        return state.output == output;
    });
    updateConfig();
}

wlr_output_manager_v1 *WOutputManagerV1::handle() const
{
    return nativeInterface<wlr_output_manager_v1>();
}

QByteArrayView WOutputManagerV1::interfaceName() const
{
    return "zwlr_output_manager_v1";
}

void WOutputManagerV1::create(WServer *server)
{
    W_D(WOutputManagerV1);
    auto *manager = wlr_output_manager_v1_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->testListener.connect(&manager->events.test, [d](void *data) {
        d->outputMgrApplyOrTest(static_cast<wlr_output_configuration_v1 *>(data), true);
    });
    d->applyListener.connect(&manager->events.apply, [d](void *data) {
        d->outputMgrApplyOrTest(static_cast<wlr_output_configuration_v1 *>(data), false);
    });
}

void WOutputManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WOutputManagerV1);
    d->testListener.disconnect();
    d->applyListener.disconnect();
    d->releasePendingConfigurations();
    m_handle = nullptr;
}

wl_global *WOutputManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
