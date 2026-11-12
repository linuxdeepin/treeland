// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

class WaylandServer;
class OutputManager;
class SurfaceWrapper;

class ShowDesktop : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Normal,
        Show,
    };

    ShowDesktop(WaylandServer *server, OutputManager *outputManager);

    State state() const;
    void setState(State state);
    void cancelShowDesktop(SurfaceWrapper *excludeSurface = nullptr);
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void restoreShowDesktopFocus();

Q_SIGNALS:
    void stateChanged();

private:
    WaylandServer *m_server = nullptr;
    OutputManager *m_outputManager = nullptr;
    State m_showDesktop = State::Normal;
};
