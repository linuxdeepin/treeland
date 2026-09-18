// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test application for treeland_active_notify_manager_v1
// (treeland-active-notify-unstable-v1). Creates a seat activity notifier
// for the current seat and prints activity_changed / drag_changed events
// to the console.

#include "qwayland-treeland-active-notify-unstable-v1.h"

#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QWaylandClientExtension>

class ActiveNotify
    : public QObject
    , public QtWayland::treeland_active_notify_v1
{
    Q_OBJECT
public:
    explicit ActiveNotify(struct ::treeland_active_notify_v1 *id, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_active_notify_v1(id)
    {
    }

    ~ActiveNotify() override
    {
        destroy();
    }

protected:
    void treeland_active_notify_v1_activity_changed(uint32_t reason, uint32_t state) override
    {
        const QString reasonName = reason == QtWayland::treeland_active_notify_v1::reason_mouse
            ? QStringLiteral("mouse")
            : QStringLiteral("wheel");
        const QString stateName = state == QtWayland::treeland_active_notify_v1::activity_state_active
            ? QStringLiteral("active")
            : QStringLiteral("inactive");
        qWarning() << "activity_changed:" << reasonName << stateName;
    }

    void treeland_active_notify_v1_drag_changed(uint32_t state) override
    {
        QString stateName;
        switch (state) {
        case QtWayland::treeland_active_notify_v1::drag_state_started:
            stateName = QStringLiteral("started");
            break;
        case QtWayland::treeland_active_notify_v1::drag_state_dropped:
            stateName = QStringLiteral("dropped");
            break;
        case QtWayland::treeland_active_notify_v1::drag_state_cancelled:
            stateName = QStringLiteral("cancelled");
            break;
        default:
            stateName = QStringLiteral("unknown");
            break;
        }
        qWarning() << "drag_changed:" << stateName;
    }
};

class TreelandActiveNotifyManagerV1
    : public QWaylandClientExtensionTemplate<TreelandActiveNotifyManagerV1>
    , public QtWayland::treeland_active_notify_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;

    explicit TreelandActiveNotifyManagerV1()
        : QWaylandClientExtensionTemplate<TreelandActiveNotifyManagerV1>(InterfaceVersion)
    {
    }

    ActiveNotify *createActiveNotify(struct ::wl_seat *seat, QObject *parent = nullptr)
    {
        if (m_activeNotify)
            return m_activeNotify;
        auto *raw = get_active_notify(seat);
        if (!raw)
            return nullptr;
        m_activeNotify = new ActiveNotify(raw, parent);
        return m_activeNotify;
    }

private:
    QPointer<ActiveNotify> m_activeNotify;
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    auto *manager = new TreelandActiveNotifyManagerV1;
    QObject::connect(manager, &TreelandActiveNotifyManagerV1::activeChanged, manager, [manager] {
        if (!manager->isActive()) {
            return;
        }

        auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!waylandApp) {
            return;
        }
        auto seat = waylandApp->seat();

        if (!seat)
            qFatal("Failed to get wl_seat from QtWayland QPA!");

        [[maybe_unused]] ActiveNotify *notify = manager->createActiveNotify(seat, manager);
    });

    return app.exec();
}

#include "main.moc"
