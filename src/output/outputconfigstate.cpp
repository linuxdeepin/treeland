// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputconfigstate.h"

void OutputConfigState::markScreenAsPrimary(const QString &outputName)
{
    if (m_states.contains(outputName)) {
        m_states[outputName].wasPrimary = true;
    } else {
        m_states[outputName] = OutputPrimaryState{true};
    }
}

bool OutputConfigState::wasScreenPrimary(const QString &outputName) const
{
    if (m_states.contains(outputName)) {
        return m_states[outputName].wasPrimary;
    }
    return false;
}

void OutputConfigState::clearOutputState(const QString &outputName)
{
    m_states.remove(outputName);
}

void OutputConfigState::recordCopyModeExit()
{
    m_copyModeExited = true;
}

bool OutputConfigState::shouldRestoreCopyMode() const
{
    return m_copyModeExited;
}

void OutputConfigState::clearCopyModeIntent()
{
    m_copyModeExited = false;
}
