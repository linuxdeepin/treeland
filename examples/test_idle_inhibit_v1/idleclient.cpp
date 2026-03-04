// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idleclient.h"

#include <private/qwaylandwindow_p.h>

#include <QDBusInterface>
#include <QDBusReply>
#include <QLabel>
#include <QProcess>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

IdleClient::IdleClient(QObject *parent)
    : QObject(parent)
    , m_inhibitWindow(new QWidget())
{
}

IdleClient::~IdleClient()
{
    m_inhibitWindow->deleteLater();
}

bool IdleClient::initialize(uint32_t idleTimeout,
                            uint32_t inhibitDuration,
                            uint32_t dbusInhibitDuration,
                            const QString &execCommand)
{
    m_idleTimeout = idleTimeout;
    m_inhibitDuration = inhibitDuration;
    m_dbusInhibitDuration = dbusInhibitDuration;
    m_execCommand = execCommand;

    auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp) {
        qWarning("Not a Wayland application");
        return false;
    }

    auto seat = waylandApp->seat();
    if (!seat) {
        qWarning("Failed to get wl_seat from QtWayland QPA");
        return false;
    }

    m_notifier.reset(new IdleNotifierV1);
    m_notifier->instantiate();

    if (!m_notifier->isInitialized()) {
        qWarning("Failed to initialize IdleNotifierV1");
        return false;
    }

    m_notification = m_notifier->getIdleNotification(m_idleTimeout, seat);
    if (!m_notification) {
        qWarning("Failed to create IdleNotificationV1");
        return false;
    }

    connect(m_notification.get(), &IdleNotificationV1::idled, this, &IdleClient::onIdled);
    connect(m_notification.get(), &IdleNotificationV1::resumed, this, &IdleClient::onResumed);

    if (m_inhibitDuration > 0) {
        m_inhibitManager.reset(new IdleInhibitManagerV1);
        m_inhibitManager->instantiate();

        if (!m_inhibitManager->isInitialized()) {
            qWarning("Failed to initialize IdleInhibitManagerV1");
        }

        m_inhibitTimer = new QTimer(this);
        m_inhibitTimer->setSingleShot(true);
        connect(m_inhibitTimer, &QTimer::timeout, this, &IdleClient::releaseInhibit);

        setupInhibit();
        qInfo() << "Idle inhibitor created at startup, will be released after" << m_inhibitDuration
                << "ms";
        m_inhibitTimer->start(m_inhibitDuration);
    } else if (m_dbusInhibitDuration > 0) {
        m_inhibitTimer = new QTimer(this);
        m_inhibitTimer->setSingleShot(true);
        connect(m_inhibitTimer, &QTimer::timeout, this, &IdleClient::releaseDBusInhibit);

        setupDBusInhibit();
        qInfo() << "Idle inhibitor created at startup using org.freedesktop.ScreenSaver D-Bus "
                   "interface, will be released after"
                << m_dbusInhibitDuration << "ms";
        m_inhibitTimer->start(m_dbusInhibitDuration);
    }

    qInfo() << "IdleClient initialized with timeout:" << m_idleTimeout << "ms, inhibit duration:"
            << (m_inhibitDuration > 0 ? m_inhibitDuration : m_dbusInhibitDuration) << "ms";
    if (!m_execCommand.isEmpty()) {
        qInfo() << "Exec command on idle:" << m_execCommand;
    }
    return true;
}

bool IdleClient::isValid() const
{
    return m_notifier && m_notifier->isInitialized() && m_notification;
}

void IdleClient::onIdled()
{
    m_isIdle = true;
    qInfo() << "System is now idle";
    Q_EMIT idleStateChanged(true);

    if (!m_execCommand.isEmpty()) {
        executeCommand();
    }
}

void IdleClient::onResumed()
{
    m_isIdle = false;
    qInfo() << "System resumed from idle";
    Q_EMIT idleStateChanged(false);
}

void IdleClient::setupInhibit()
{
    if (m_inhibitor) {
        return;
    }

    m_inhibitWindow->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_inhibitWindow->setWindowTitle("Idle Inhibitor");
    m_inhibitWindow->resize(100, 100);

    QVBoxLayout *layout = new QVBoxLayout(m_inhibitWindow);
    QLabel *label = new QLabel("Idle Inhibitor", m_inhibitWindow);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    m_inhibitWindow->show();

    QMetaObject::invokeMethod(
        this,
        [this]() {
            QWidget *widget = m_inhibitWindow;

            QWindow *window = widget->windowHandle();

            if (!window || !window->handle()) {
                qWarning() << "Window handle not available";
                return;
            }

            QtWaylandClient::QWaylandWindow *waylandWindow =
                static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
            struct ::wl_surface *surface = waylandWindow->wlSurface();

            if (!surface) {
                qWarning() << "Failed to get wl_surface for window";
                return;
            }

            m_inhibitor = m_inhibitManager->createInhibitor(surface);
            if (m_inhibitor) {
                m_inhibitActive = true;
                qInfo() << "Idle inhibitor created successfully (QWidget with content)";
            } else {
                qWarning() << "Failed to create idle inhibitor";
            }
        },
        Qt::QueuedConnection);
}

void IdleClient::releaseInhibit()
{
    if (m_inhibitor) {
        m_inhibitor.reset();
        m_inhibitActive = false;
        qInfo() << "Idle inhibitor released";
    }
}

void IdleClient::setupDBusInhibit()
{
    QDBusInterface interface("org.freedesktop.ScreenSaver",
                             "/org/freedesktop/ScreenSaver",
                             "org.freedesktop.ScreenSaver",
                             QDBusConnection::sessionBus());
    if (!interface.isValid()) {
        qWarning() << "Failed to connect to org.freedesktop.ScreenSaver:"
                   << QDBusConnection::sessionBus().lastError().message();
        return;
    }
    QDBusReply<uint32_t> reply =
        interface.call("Inhibit", "test-idle-inhibit-v1", "Testing idle inhibit");
    if (reply.isValid()) {
        m_dbusInhibitCookie = reply.value();
        qInfo() << "Inhibited successfully using org.freedesktop.ScreenSaver D-Bus interface. Cookie:" << m_dbusInhibitCookie;
    } else {
        qWarning() << "Failed to call Inhibit:" << reply.error().message();
    }
}

void IdleClient::releaseDBusInhibit()
{
    if (m_dbusInhibitCookie > 0) {
        QDBusInterface interface("org.freedesktop.ScreenSaver",
                                 "/org/freedesktop/ScreenSaver",
                                 "org.freedesktop.ScreenSaver",
                                 QDBusConnection::sessionBus());
        if (!interface.isValid()) {
            qWarning() << "Failed to connect to org.freedesktop.ScreenSaver:"
                       << QDBusConnection::sessionBus().lastError().message();
            return;
        }
        QDBusReply<void> reply = interface.call("UnInhibit", m_dbusInhibitCookie);
        if (reply.isValid()) {
            qInfo() << "Idle inhibitor released using org.freedesktop.ScreenSaver D-Bus interface.";
            m_dbusInhibitCookie = 0;
        } else {
            qWarning() << "Failed to call UnInhibit:" << reply.error().message();
        }
    }
}

void IdleClient::executeCommand()
{
    qInfo() << "Executing command:" << m_execCommand;

    QProcess *process = new QProcess(this);
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [](int exitCode, QProcess::ExitStatus exitStatus) {
                qInfo() << "Command finished with exit code:" << exitCode;
                if (exitStatus == QProcess::CrashExit) {
                    qWarning() << "Command crashed";
                }
            });

    process->start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), m_execCommand });
}
