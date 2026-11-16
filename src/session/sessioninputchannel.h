// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"

#include <wglobal.h>

#include <QHash>
#include <QSet>
#include <QPointer>

class WaylandServer;
class InputManager;
class GestureManager;
class ShortcutController;
class ShortcutManagerV3;
class CompositorInputFilter;
QT_BEGIN_NAMESPACE
class QInputEvent;
QT_END_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

class SessionInputChannel : public SessionComponent
{
    Q_OBJECT
public:
    SessionInputChannel(Session *session, WaylandServer *runtime);
    ~SessionInputChannel() override;

    CompositorInputFilter *filter() const { return m_filter; };
    GestureManager *gestures() const { return m_gestures; };
    ShortcutController *shortcuts() const { return m_shortcuts; };
    ShortcutManagerV3 *shortcutProtocol() const;

protected:
    void activate() override;
    void deactivate() override;

private:
    bool admitEvent(WAYLIB_SERVER_NAMESPACE::WSeat *seat, QInputEvent *event);

private:
    WaylandServer *const m_runtime = nullptr;
    QPointer<InputManager> m_input;
    GestureManager *const m_gestures = nullptr;
    ShortcutController *const m_shortcuts = nullptr;
    CompositorInputFilter *const m_filter = nullptr;
    QHash<WAYLIB_SERVER_NAMESPACE::WSeat *, QSet<uint32_t>> m_suppressedKeys;
    Qt::MouseButtons m_suppressedButtons;
    QPointer<ShortcutManagerV3> m_shortcutProtocol;
    bool m_touchStarted = false;
};
