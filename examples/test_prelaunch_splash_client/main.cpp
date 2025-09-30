// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// QtWayland C++ 封装版本的预启动闪屏 demo
// 使用方式: test-prelaunch-splash-client <app-id> [command-to-launch]
// 若未提供 command-to-launch 则默认使用 dde-am <app-id>

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QtWaylandClient/QWaylandClientExtension>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "qwayland-treeland-prelaunch-splash-v1.h"

class PrelaunchSplashManager
    : public QWaylandClientExtensionTemplate<PrelaunchSplashManager>
    , public QtWayland::treeland_prelaunch_splash_manager_v1
{
    Q_OBJECT
public:
    explicit PrelaunchSplashManager()
        : QWaylandClientExtensionTemplate<PrelaunchSplashManager>(1) // protocol version 1
    {
    }

    // activeChanged 信号来自 QWaylandClientExtensionTemplate
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    if (argc < 2) {
        qCritical() << "Usage:" << argv[0] << "<app-id> [command-to-launch]";
        return 1;
    }
    QString appId = QString::fromUtf8(argv[1]);
    QString launchCmd;
    if (argc >= 3) {
        launchCmd = QString::fromUtf8(argv[2]);
    } else {
        launchCmd = appId; // 默认同名
    }

    PrelaunchSplashManager manager;
    QObject::connect(&manager, &PrelaunchSplashManager::activeChanged, &manager, [&] {
        if (!manager.isActive())
            return;
        // 绑定成功后发送 create_splash
        manager.create_splash(appId);
        qInfo() << "Sent create_splash for" << appId;

        // 启动应用（可选）
        if (!launchCmd.isEmpty()) {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("dde-am", "dde-am", launchCmd.toUtf8().constData(), (char*)nullptr);
                _exit(127);
            } else if (pid > 0) {
                qInfo() << "Launched via dde-am" << launchCmd;
            } else {
                qWarning() << "fork failed";
            }
        }
        // 稍后退出（仅示例用途）
        QTimer::singleShot(300, &app, &QCoreApplication::quit);
    });

    return app.exec();
}

#include "main.moc"
