// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputmanagerv1.h"
#include "woutputitem.h"
#include "private/wglobal_p.h"

#include <qwoutput.h>
#include <qwoutputmanagementv1.h>
#include <qwdisplay.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::qw_output_manager_v1;
using QW_NAMESPACE::qw_output_configuration_v1;
using QW_NAMESPACE::qw_output_configuration_head_v1;

class Q_DECL_HIDDEN WOutputManagerV1Private : public WObjectPrivate
{
public:
    WOutputManagerV1Private(WOutputManagerV1 *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WOutputManagerV1)

    void outputMgrApplyOrTest(qw_output_configuration_v1 *config, int test);
    inline qw_output_manager_v1 *handle() const {
        return q_func()->nativeInterface<qw_output_manager_v1>();
    }

    inline wlr_output_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    qw_output_manager_v1 *manager { nullptr };
    QPointer<WBackend> backend;
    QList<WOutputState> stateList;
    QList<WOutputState> stateListPending;
};

WOutputManagerV1::WOutputManagerV1()
    : WObject(*new WOutputManagerV1Private(this))
{

}

void WOutputManagerV1Private::outputMgrApplyOrTest(qw_output_configuration_v1 *config, int onlyTest)
{
    W_Q(WOutputManagerV1);
    wlr_output_configuration_head_v1 *config_head;

    stateListPending.clear();

    wl_list_for_each(config_head, &config->handle()->heads, link) {
        auto *output = QW_NAMESPACE::qw_output::from(config_head->state.output);
        auto *woutput = WOutput::fromHandle(output);

        const auto &state = config_head->state;

        stateListPending.append(WOutputState {
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

    Q_EMIT q->requestTestOrApply(config, onlyTest);
}

const QList<WOutputState> &WOutputManagerV1::stateListPending()
{
    W_D(WOutputManagerV1);
    return d->stateListPending;
}

void WOutputManagerV1::updateConfig()
{
    W_D(WOutputManagerV1);

    auto *config = qw_output_configuration_v1::create();
    for (const WOutputState &state : std::as_const(d->stateList)) {
        auto *wlr_output = state.output->nativeHandle();
        auto *configHead = qw_output_configuration_head_v1::create(*config, wlr_output);
        auto *handle = configHead->handle();

        handle->state.enabled = state.enabled;
        handle->state.x = state.x;
        handle->state.y = state.y;

        if (state.enabled) {
            handle->state.mode = state.mode;
            handle->state.scale = state.scale;
            handle->state.transform = static_cast<wl_output_transform>(state.transform);
            handle->state.adaptive_sync_enabled = state.adaptiveSyncEnabled;

            if (state.customModeSize.width() > 0 && state.customModeSize.height() > 0) {
                handle->state.custom_mode.width = state.customModeSize.width();
                handle->state.custom_mode.height = state.customModeSize.height();
                handle->state.custom_mode.refresh = state.customModeRefresh;
            }
        }
    }

    d->manager->set_configuration(*config);
}

void WOutputManagerV1::sendResult(qw_output_configuration_v1 *config, bool ok)
{
    W_D(WOutputManagerV1);

    if (ok) {
        config->send_succeeded();
        d->stateList = d->stateListPending;
    } else {
        config->send_failed();
    }

    delete config;
    d->stateListPending.clear();

    // Schedule updateConfig through the event loop to avoid recursion
    QMetaObject::invokeMethod(this, &WOutputManagerV1::updateConfig, Qt::QueuedConnection);
}

void WOutputManagerV1::newOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    const auto *wlr_output = output->nativeHandle();

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

qw_output_manager_v1 *WOutputManagerV1::handle() const
{
    return nativeInterface<qw_output_manager_v1>();
}

QByteArrayView WOutputManagerV1::interfaceName() const
{
    return "zwlr_output_head_v1";
}

void WOutputManagerV1::create(WServer *server)
{
    W_D(WOutputManagerV1);

    d->manager = qw_output_manager_v1::create(*server->handle());
    connect(d->manager, &qw_output_manager_v1::notify_test, this, [d](wlr_output_configuration_v1 *config) {
        d->outputMgrApplyOrTest(qw_output_configuration_v1::from(config), true);
    });

    connect(d->manager, &qw_output_manager_v1::notify_apply, this, [d](wlr_output_configuration_v1 *config) {
        d->outputMgrApplyOrTest(qw_output_configuration_v1::from(config), false);
    });
}

wl_global *WOutputManagerV1::global() const
{
    W_D(const WOutputManagerV1);

    if (m_handle)
        return d->nativeHandle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
