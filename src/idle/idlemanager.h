// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wpointer.h>
#include <wscoplistener.h>

#include <QElapsedTimer>
#include <QList>
#include <QObject>

struct wlr_surface;

struct wlr_idle_notifier_v1;
struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_inhibitor_v1;
class ScreensaverInterfaceV2;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class IdleManager : public QObject, public WObject
{
    Q_OBJECT
public:
    explicit IdleManager(QObject *parent = nullptr);
    ~IdleManager() override;

    void initialize(WServer *server);
    void setScreensaver(ScreensaverInterfaceV2 *screensaver);
    void notifyActivity(WSeat *seat);
    void updateIdleInhibitor();

    // One source currently holding idle off, either a
    // zwp_idle_inhibit_manager_v1 surface or a treeland_screensaver_v2
    // inhibit. Exposed to the treeland-debug ``idle`` command.
    struct Inhibitor {
        QString kind;                    // "idle-inhibit" or "screensaver"
        wlr_surface *surface = nullptr;  // idle-inhibit only
        QString appId;
        QString title;
        int pid = 0;
        QString appName;  // screensaver only (client-provided)
        QString reason;   // screensaver only (client-provided)
        bool active = false;  // this source alone counts as inhibiting
        quint64 heldMs = 0;   // how long the source has been held
    };

    // True while idle notification is suppressed by any source.
    bool isIdleInhibited() const { return m_inhibited; }
    bool isScreensaverInhibited() const;
    // Milliseconds since the last input activity on any seat; counted from
    // compositor start until the first activity is seen.
    qint64 idleMs() const;
    QList<Inhibitor> inhibitors() const;

private:
    struct IdleInhibitorEntry {
        wlr_idle_inhibitor_v1 *inhibitor = nullptr;
        QElapsedTimer age;
        WScopedListener destroyListener;
        // Watches the native surface until it gets a WSurface wrapper (see
        // bindIdleInhibitorSurface()); disconnected once bound.
        WScopedListener commitListener;
        QList<QMetaObject::Connection> connections;
    };

    void onNewIdleInhibitor(wlr_idle_inhibitor_v1 *inhibitor);
    void bindIdleInhibitorSurface(IdleInhibitorEntry &entry);

    ScreensaverInterfaceV2 *m_screensaver = nullptr;
    wlr_idle_notifier_v1 *m_idleNotifier = nullptr;
    WPointer<wlr_idle_inhibit_manager_v1> m_idleInhibitManager;
    bool m_inhibited = false;
    QElapsedTimer m_sinceActivity;
    std::vector<IdleInhibitorEntry> idleInhibitorEntries;
};
