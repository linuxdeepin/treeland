// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickoutputmanager_p.h"
#include <qwoutput.h>

extern "C" {
#define static
#include <wlr/types/wlr_output_management_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWOutputManagerV1;
using QW_NAMESPACE::QWOutputConfigurationV1;
using QW_NAMESPACE::QWOutputConfigurationHeadV1;

class WQuickOutputManagerPrivate : public WObjectPrivate
{
public:
    WQuickOutputManagerPrivate(WQuickOutputManager *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickOutputManager)

    void outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int test);

    QWOutputManagerV1 *manager { nullptr };
    QPointer<WBackend> backend;
    QList<WOutputState> stateList;
    QList<WOutputState> stateListPending;
};

WQuickOutputManager::WQuickOutputManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickOutputManagerPrivate(this), nullptr)
{

}

void WQuickOutputManager::create()
{
    W_D(WQuickOutputManager);
    WQuickWaylandServerInterface::create();

    d->manager = QWOutputManagerV1::create(server()->handle());
    connect(d->manager, &QWOutputManagerV1::test, this, [d](QWOutputConfigurationV1 *config) {
        d->outputMgrApplyOrTest(config, true);
    });

    connect(d->manager, &QWOutputManagerV1::apply, this, [d](QWOutputConfigurationV1 *config) {
        d->outputMgrApplyOrTest(config, false);
    });
}

void WQuickOutputManagerPrivate::outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int onlyTest)
{
    W_Q(WQuickOutputManager);
    wlr_output_configuration_head_v1 *config_head;
    int ok = 1;

    stateListPending.clear();

    wl_list_for_each(config_head, &config->handle()->heads, link) {
        auto *output = QW_NAMESPACE::QWOutput::from(config_head->state.output);
        auto *woutput = WOutput::fromHandle(output);

        const auto &state = config_head->state;

        stateListPending.append(WOutputState {
            .m_output = woutput,
            .m_enabled = state.enabled,
            .m_mode = state.mode,
            .m_x = state.x,
            .m_y = state.y,
            .m_custom_mode_size = { state.custom_mode.width, state.custom_mode.height },
            .m_custom_mode_refresh = state.custom_mode.refresh,
            .m_transform = static_cast<WOutput::Transform>(state.transform),
            .m_scale = state.scale,
            .m_adaptive_sync_enabled = state.adaptive_sync_enabled
        });
    }

    Q_EMIT q->requestTestOrApply(config, onlyTest);
}

QList<WOutputState> WQuickOutputManager::stateListPending()
{
    W_D(WQuickOutputManager);
    return d->stateListPending;
}

void WQuickOutputManager::updateConfig()
{
    W_D(WQuickOutputManager);

    auto *config = QWOutputConfigurationV1::create();

    for (const WOutputState &state : d->stateList) {
        auto *configHead = QWOutputConfigurationHeadV1::create(config, state.m_output->handle());
        configHead->handle()->state.scale = state.m_scale;
        configHead->handle()->state.transform = static_cast<wl_output_transform>(state.m_transform);
        configHead->handle()->state.x = state.m_x;
        configHead->handle()->state.y = state.m_y;
    }

    d->manager->setConfiguration(config);
}

void WQuickOutputManager::sendResult(QWOutputConfigurationV1 *config, bool ok)
{
    W_D(WQuickOutputManager);
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

void WQuickOutputManager::newOutput(WOutput *output)
{
    W_D(WQuickOutputManager);
    const auto *wlr_output = output->nativeHandle();
    WOutputState state {
        .m_output = output,
        .m_enabled = wlr_output->enabled,
        .m_mode = wlr_output->current_mode,
        .m_x = 0,
        .m_y = 0,
        .m_custom_mode_size = {  wlr_output->width,  wlr_output->height },
        .m_custom_mode_refresh =  wlr_output->refresh,
        .m_transform = static_cast<WOutput::Transform>(wlr_output->transform),
        .m_scale = wlr_output->scale,
        .m_adaptive_sync_enabled = (wlr_output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED)
    };
    d->stateList.append(state);
    updateConfig();
}

void WQuickOutputManager::removeOutput(WOutput *output)
{
    W_D(WQuickOutputManager);
    d->stateList.removeIf([output](const WOutputState &s) {
        return s.m_output == output;
    });

    updateConfig();
}

WAYLIB_SERVER_END_NAMESPACE
