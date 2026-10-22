// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/show-desktop/showdesktopinterfacev1.h"

#include <QList>
#include <QPointer>

class WaylandServer;
class OutputManager;
class SurfaceWrapper;

class ShowDesktop : public QObject
{
    Q_OBJECT
public:
    ShowDesktop(WaylandServer *server, OutputManager *outputManager);

    void initialize(WServer *server);
    void connectProtocol();
    bool isAvailable() const;
    ShowDesktopInterfaceV1::State state() const;
    void setState(ShowDesktopInterfaceV1::State state);
    void cancelShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void restoreShowDesktopFocus();

Q_SIGNALS:
    void stateChanged();

private:
    void onShowDesktop();

private:
    WaylandServer *m_server = nullptr;
    OutputManager *m_outputManager = nullptr;
    ShowDesktopInterfaceV1 *m_showDesktopInterfaceV1 = nullptr;
    ShowDesktopInterfaceV1::State m_showDesktop = ShowDesktopInterfaceV1::State::Normal;
    QList<QPointer<SurfaceWrapper>> m_hiddenSurfaces;
};
