// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "output/output.h"

#include "sessioncomponent.h"
#include <wglobal.h>
#include <functional>
#include <cstdint>

class Output;
class OutputManager;
class RootSurfaceContainer;
struct wlr_output_configuration_v1;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputManagerV1;
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

// Hardware authorization boundary. System display settings and logical outputs do not
// inspect login identity or decide whether hardware is connected.
class SessionOutputChannel : public SessionComponent
{
    Q_OBJECT
public:
    SessionOutputChannel(Session *session, OutputManager *hardware, RootSurfaceContainer *root);
    void setScale(WAYLIB_SERVER_NAMESPACE::WOutput *output, qreal scale);
    void setPrimary(Output *output);
    void applyColor(Output *output, qreal brightness, uint32_t temperature,
                    std::function<void(Output::CommitColorResult)> completed);
    void configure(WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *protocol,
                   wlr_output_configuration_v1 *configuration, bool onlyTest);
Q_SIGNALS:
    void connected();
protected:
    void activate() override;
private:
    OutputManager *const m_hardware;
    RootSurfaceContainer *const m_root;
};
