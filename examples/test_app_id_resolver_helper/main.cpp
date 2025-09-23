// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// QtWayland C++ 扩展版 app-id resolver helper（作为特权解析端）
// 逻辑：
// 1. 通过 QWaylandClientExtensionTemplate 绑定 treeland_app_id_resolver_manager_v1
// 2. active 后获取 resolver 对象
// 3. 在 treeland_app_id_resolver_v1_identify_request 回调中通过 DBus(pidfd) 解析 appId 并 respond

#include <QGuiApplication>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QDBusMessage>
#include <QtWaylandClient/QWaylandClientExtension>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "qwayland-treeland-app-id-resolver-v1.h"

static QString identifyViaDBus(int pidfd)
{
    if (pidfd < 0)
        return {};
    int dupfd = fcntl(pidfd, F_DUPFD_CLOEXEC, 0);
    if (dupfd < 0) {
        qWarning() << "dup pidfd failed" << strerror(errno);
        return {};
    }
    QDBusUnixFileDescriptor wrapper(dupfd);
    close(dupfd);
    auto msg = QDBusMessage::createMethodCall(
        "org.desktopspec.ApplicationManager1",
        "/org/desktopspec/ApplicationManager1",
        "org.desktopspec.ApplicationManager1",
        "Identify");
    msg << QVariant::fromValue(wrapper);
    auto reply = QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qWarning() << "DBus Identify failed" << reply.errorMessage();
        return {};
    }
    const QString appId = reply.arguments().first().toString();
    if (appId.isEmpty())
        qWarning() << "Empty appId";
    return appId;
}

class AppIdResolver : public QObject, public QtWayland::treeland_app_id_resolver_v1
{
    Q_OBJECT
public:
    explicit AppIdResolver(struct ::treeland_app_id_resolver_v1 *object, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_app_id_resolver_v1(object) {}

protected:
    void treeland_app_id_resolver_v1_identify_request(uint32_t request_id, int32_t pidfd) override
    {
        qInfo() << "identify_request" << request_id << pidfd;
        const QString appId = identifyViaDBus(pidfd);
        respond(request_id, appId);
        qInfo() << "respond sent" << request_id << appId;
    }
};

class AppIdResolverManager
    : public QWaylandClientExtensionTemplate<AppIdResolverManager>
    , public QtWayland::treeland_app_id_resolver_manager_v1
{
    Q_OBJECT
public:
    AppIdResolverManager()
        : QWaylandClientExtensionTemplate<AppIdResolverManager>(1) // version 1
    {
    }

    void ensureResolver()
    {
        if (m_resolver)
            return;
        auto *raw = get_resolver();
        if (!raw) {
            qWarning() << "get_resolver returned null";
            return;
        }
        m_resolver = new AppIdResolver(raw, this);
        qInfo() << "Resolver object created";
    }

private:
    AppIdResolver *m_resolver = nullptr;
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    AppIdResolverManager manager;
    QObject::connect(&manager, &AppIdResolverManager::activeChanged, &manager, [&manager] {
        if (manager.isActive()) {
            qInfo() << "Resolver manager active";
            manager.ensureResolver();
        } else {
            qWarning() << "Resolver manager inactive";
        }
    });

    qInfo() << "AppId resolver helper running (QtWayland extension mode)";
    return app.exec();
}

#include "main.moc"
