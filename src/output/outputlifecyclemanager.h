// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QList>
#include <QString>
#include <QObject>

#include <memory>

class Output;
class OutputConfigState;
class SurfaceWrapper;
class RootSurfaceContainer;

class OutputLifecycleManager : public QObject {
    Q_OBJECT
public:
    enum class Mode
    {
        Extension, // Independent displays
        Copy       // Mirrored displays
    };

    explicit OutputLifecycleManager(RootSurfaceContainer *rootContainer,
                                   OutputConfigState *configState,
                                   QObject *parent = nullptr);
    ~OutputLifecycleManager() = default;

    void onScreenAdded(Output *output);
    void onScreenRemoved(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenDisabled(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenEnabled(Output *output);
    void setMode(Mode mode) {
        m_mode = mode;
    }

    Mode getMode() const {
        return m_mode;
    }

    bool takeCopyModeRestoreIntent() {
        bool result = m_copyModeRestoreIntent;
        m_copyModeRestoreIntent = false;
        return result;
    }

private:
    RootSurfaceContainer *m_rootContainer = nullptr;
    OutputConfigState *m_configState = nullptr;
    Mode m_mode = Mode::Extension;
    bool m_copyModeRestoreIntent = false;

    Output *findFirstAvailableOutput(Output *excludeOutput);
    void markScreenAsPrimaryIntent(Output *output);
    void restoreScreenAsPrimary(Output *output);
    void switchPrimaryOutput(Output *from, Output *to, const QList<SurfaceWrapper *> &surfaces);
};
