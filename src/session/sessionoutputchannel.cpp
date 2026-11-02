// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <utility>
#include "sessionoutputchannel.h"
#include "output/outputmanager.h"
#include "output/output.h"
#include "core/rootsurfacecontainer.h"
#include <woutputmanagerv1.h>

SessionOutputChannel::SessionOutputChannel(Session *session, OutputManager *hardware,
                                         RootSurfaceContainer *root)
    : SessionComponent(session), m_hardware(hardware), m_root(root)
{
}

void SessionOutputChannel::activate()
{
    m_hardware->selectSession(session());
    Q_EMIT connected();
}

void SessionOutputChannel::setScale(WAYLIB_SERVER_NAMESPACE::WOutput *output, qreal scale)
{
    if (isActive())
        m_hardware->setScale(output, scale);
}

void SessionOutputChannel::setPrimary(Output *output)
{
    if (isActive())
        m_root->setPrimaryOutput(output, true);
}

void SessionOutputChannel::applyColor(Output *output, qreal brightness, uint32_t temperature,
                                     std::function<void(Output::CommitColorResult)> completed)
{
    if (!isActive()) {
        if (completed)
            completed(Output::CommitColorResult::Failed);
        return;
    }
    output->setOutputColor(brightness, temperature, std::move(completed));
}

void SessionOutputChannel::configure(WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *protocol,
                                    wlr_output_configuration_v1 *configuration, bool onlyTest)
{
    if (!isActive()) {
        protocol->sendResult(configuration, false);
        return;
    }
    m_hardware->testOrApplyConfiguration(protocol, configuration, onlyTest);
}
