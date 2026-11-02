// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QPointer>
#include <QQuickItem>

class ShortcutController;
class ShortcutManagerV3;
class ShortcutRunner;
class Output;
class Workspace;
class QmlEngine;
class WaylandServer;
class DesktopEnvironment;
class CompositorInputFilter;
class SessionSwitcher : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    SessionSwitcher(Workspace *workspace, QmlEngine *engine, WaylandServer *runtime,
                    DesktopEnvironment *desktop);
    void activate();
    void deactivate();
    void setInputFilter(CompositorInputFilter *filter);
    void setShortcutController(ShortcutController *controller, ShortcutManagerV3 *protocol);
    QQuickItem *taskSwitcher() const;
    QQuickItem *ensureTaskSwitcher(Output *output);

public Q_SLOTS:
    void close();

private:
    Workspace *const m_workspace;
    QmlEngine *const m_engine;
    WaylandServer *const m_runtime;
    DesktopEnvironment *const m_desktop;
    CompositorInputFilter *m_inputFilter = nullptr;
    bool m_running = false;
    ShortcutRunner *m_shortcuts = nullptr;
    QPointer<QQuickItem> m_taskSwitcher;
};
