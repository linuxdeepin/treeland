// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputmanagerv1.h"
#include "woutputitem.h"
#include "woutputitem_p.h"
#include "private/wglobal_p.h"

#include <qwoutput.h>
#include <qwoutputmanagementv1.h>

extern "C" {
#define static
#include <wlr/types/wlr_output_management_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWOutputManagerV1;
using QW_NAMESPACE::QWOutputConfigurationV1;
using QW_NAMESPACE::QWOutputConfigurationHeadV1;

class WOutputManagerV1Private : public WObjectPrivate
{
public:
    WOutputManagerV1Private(WOutputManagerV1 *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WOutputManagerV1)

    void outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int test);
    inline QWOutputManagerV1 *handle() const {
        return q_func()->nativeInterface<QWOutputManagerV1>();
    }

    inline wlr_output_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    QWOutputManagerV1 *manager { nullptr };
    QPointer<WBackend> backend;
    QList<WOutputState> stateList;
    QList<WOutputState> stateListPending;
};

WOutputManagerV1::WOutputManagerV1()
    : WObject(*new WOutputManagerV1Private(this))
{

}

void WOutputManagerV1Private::outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int onlyTest)
{
    W_Q(WOutputManagerV1);
    wlr_output_configuration_head_v1 *config_head;

    stateListPending.clear();

    wl_list_for_each(config_head, &config->handle()->heads, link) {
        auto *output = QW_NAMESPACE::QWOutput::from(config_head->state.output);
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

QList<WOutputState> WOutputManagerV1::stateListPending()
{
    W_D(WOutputManagerV1);
    return d->stateListPending;
}

void WOutputManagerV1::updateConfig()
{
    W_D(WOutputManagerV1);

    auto *config = QWOutputConfigurationV1::create();

    for (const WOutputState &state : d->stateList) {
        auto *configHead = QWOutputConfigurationHeadV1::create(config, state.output->handle());
        configHead->handle()->state.scale = state.scale;
        configHead->handle()->state.transform = static_cast<wl_output_transform>(state.transform);
        configHead->handle()->state.x = state.x;
        configHead->handle()->state.y = state.y;
    }

    d->manager->setConfiguration(config);
}

void WOutputManagerV1::sendResult(QWOutputConfigurationV1 *config, bool ok)
{
    W_D(WOutputManagerV1);
    if (ok)
        config->sendSucceeded();
    else
        config->sendFailed();
    delete config;

    if (ok)
        d->stateList.swap(d->stateListPending);
    d->stateListPending.clear();
    updateConfig();
}

void WOutputManagerV1::newOutput(WOutput *output)
{
    W_D(WOutputManagerV1);
    const auto *wlr_output = output->nativeHandle();

    auto *attached = output->findChild<WOutputItemAttached*>(QString(), Qt::FindDirectChildrenOnly);
    if (!attached)
        attached = WOutputItem::qmlAttachedProperties(output);

    WOutputState state {
        .output = output,
        .enabled = wlr_output->enabled,
        .mode = wlr_output->current_mode,
        .x = attached ? static_cast<int32_t>(attached->item()->x()) : 0,
        .y = attached ? static_cast<int32_t>(attached->item()->y()) : 0,
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

QWOutputManagerV1 *WOutputManagerV1::handle() const
{
    return nativeInterface<QWOutputManagerV1>();
}

void WOutputManagerV1::create(WServer *server)
{
    W_D(WOutputManagerV1);

    d->manager = QWOutputManagerV1::create(server->handle());
    connect(d->manager, &QWOutputManagerV1::test, this, [d](QWOutputConfigurationV1 *config) {
        d->outputMgrApplyOrTest(config, true);
    });

    connect(d->manager, &QWOutputManagerV1::apply, this, [d](QWOutputConfigurationV1 *config) {
        d->outputMgrApplyOrTest(config, false);
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
