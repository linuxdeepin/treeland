// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QObject>
#define private public
#include <QCoreApplication>
#include <private/qhighdpiscaling_p.h>
#undef private

#include "wserver.h"
#include "private/wserver_p.h"
#include "wsurface.h"
#include "wsocket.h"
#include "platformplugin/qwlrootsintegration.h"

#include <qwdisplay.h>
#include <qwdatadevice.h>
#include <qwprimaryselectionv1.h>
#include <qwxwaylandshellv1.h>

#include <QVector>
#include <QThread>
#include <QEvent>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QMutex>
#include <QDebug>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <private/qthread_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformthemefactory_p.h>
#include <qpa/qplatformintegrationfactory_p.h>
#include <qpa/qplatformtheme.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

static bool globalFilter(const wl_client *client,
                         const wl_global *global,
                         void *data) {
    WServerPrivate *d = reinterpret_cast<WServerPrivate*>(data);

    do {
        if (auto interface = d->q_func()->findInterface(global)) {
            auto wclient = WClient::get(client);
            if (!wclient) {
                auto client_cred = WClient::getCredentials(client);
                if (client_cred->pid == getpid()) {
                    break;
                }
            }

            Q_ASSERT(wclient);
            if (auto filter = interface->filter()) {
                return filter(wclient);
            }
        }
    } while(false);

#ifndef DISABLE_XWAYLAND
    if (wl_global_get_interface(global)->name == QByteArrayView("xwayland_shell_v1")) {
        auto shell = reinterpret_cast<wlr_xwayland_shell_v1*>(wl_global_get_user_data(global));
        return shell->client == client;
    }
#endif

    if (!d->globalFilterFunc)
        return true;

    return d->globalFilterFunc(client, global, d->globalFilterFuncData);
}

WServerPrivate::WServerPrivate(WServer *qq)
    : WObjectPrivate(qq)
{
    display.reset(new qw_display());
    wl_display_set_global_filter(display->handle(), globalFilter, this);
}

WServerPrivate::~WServerPrivate()
{

}

void WServerPrivate::init()
{
    Q_ASSERT(display);

    // free follow display
    [[maybe_unused]] auto ddm = qw_data_device_manager::create(*display);
    [[maybe_unused]] auto psm = qw_primary_selection_v1_device_manager::create(*display);

    W_Q(WServer);

    for (auto i : std::as_const(interfaceList)) {
        i->create(q);
        if (auto global = i->global())
            Q_ASSERT(wl_global_get_interface(global)->name == i->interfaceName());
    }

    loop = wl_display_get_event_loop(display->handle());
    int fd = wl_event_loop_get_fd(loop);

    auto processWaylandEvents = [this] {
        int ret = wl_event_loop_dispatch(loop, 0);
        if (ret)
            fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
        wl_display_flush_clients(display->handle());
    };

    sockNot.reset(new QSocketNotifier(fd, QSocketNotifier::Read));
    QObject::connect(sockNot.get(), &QSocketNotifier::activated, q, processWaylandEvents);

    QAbstractEventDispatcher *dispatcher = QThread::currentThread()->eventDispatcher();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, q, processWaylandEvents);

    for (auto socket : std::as_const(sockets))
        initSocket(socket);

    Q_EMIT q->started();
}

void WServerPrivate::stop()
{
    W_Q(WServer);

    if (display)
        wl_display_destroy_clients(*display);

    auto list = interfaceList;
    interfaceList.clear();
    auto i = list.crbegin();
    for (; i != list.crend(); ++i) {
        (*i)->destroy(q);
        delete *i;
    }

    sockNot.reset();
    QThread::currentThread()->eventDispatcher()->disconnect(q);
}

void WServerPrivate::initSocket(WSocket *socketServer)
{
    bool ok = socketServer->listen(display->handle());
    Q_ASSERT(ok);
}

WServer::WServer(QObject *parent)
    : WServer(*new WServerPrivate(this), parent)
{

}

WServer::~WServer()
{
    if (isRunning())
        stop();
}

WServer::WServer(WServerPrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{
}

qw_display *WServer::handle() const
{
    W_DC(WServer);
    return d->display.get();
}

void WServer::stop()
{
    W_D(WServer);

    Q_ASSERT(d->display);
    d->stop();
}

void WServer::attach(WServerInterface *interface)
{
    W_D(WServer);
    Q_ASSERT(!d->interfaceList.contains(interface));

    Q_ASSERT(interface->m_server == nullptr);
    interface->m_server = this;

    if (isRunning()) {
        Q_ASSERT(!d->pendingInterface);
        // Save to pendingInterface in order to find this
        // WServerInterface object by WServer::findInterface(wl_global)
        d->pendingInterface = interface;
        interface->create(this);
        d->pendingInterface = nullptr;

        if (auto global = interface->global())
            Q_ASSERT(wl_global_get_interface(global)->name == interface->interfaceName());
    }

    // After interface->create append to the list when server is runing
    // See WServer::findInterface(wl_global)
    d->interfaceList << interface;
}

bool WServer::detach(WServerInterface *interface)
{
    W_D(WServer);
    Q_ASSERT(interface != d->pendingInterface);
    bool ok = d->interfaceList.removeOne(interface);
    if (!ok)
        return false;

    Q_ASSERT(interface->m_server == this);
    interface->m_server = nullptr;

    if (!isRunning())
        return false;

    interface->destroy(this);
    return true;
}

const QVector<WServerInterface *> &WServer::interfaceList() const
{
    W_DC(WServer);
    return d->interfaceList;
}

QVector<WServerInterface *> WServer::findInterfaces(void *handle) const
{
    QVector<WServerInterface*> list;
    for (auto i : interfaceList()) {
        if (i->handle() == handle)
            list << i;
    }

    return list;
}

WServerInterface *WServer::findInterface(void *handle) const
{
    for (auto i : interfaceList()) {
        if (i->handle() == handle)
            return i;
    }

    return nullptr;
}

WServerInterface *WServer::findInterface(const wl_global *global) const
{
    for (const auto &i : interfaceList()) {
        if (i->global() == global)
            return i;
    }

    W_DC(WServer);

    // When call WServerInterface::create, will call wl_global_create in wlroots,
    // and will call globalFilter in libwayland(wl_global_is_visible), globalFilter
    // wants to find a WServerInterface object and use WServerInterface::filter to filter
    // the new wl_global, but during for WServerInterface::create now, so can't use
    // WServerInterface::global() to find which a WServerInterface of the new wl_global.
    if (d->pendingInterface
        && d->pendingInterface->interfaceName() == wl_global_get_interface(global)->name) {
        return d->pendingInterface;
    }

    return nullptr;
}

WServer *WServer::from(WServerInterface *interface)
{
    return interface->m_server;
}

static bool initializeQtPlatform(bool isMaster, const QStringList &parameters, std::function<void()> onInitialized)
{
    Q_ASSERT(QGuiApplication::instance() == nullptr);
    if (QGuiApplicationPrivate::platform_integration)
        return false;

    QHighDpiScaling::initHighDpiScaling();
    QHighDpiScaling::m_globalScalingActive = true; // force enable hidpi
    QGuiApplicationPrivate::platform_integration = new QWlrootsIntegration(isMaster, parameters, onInitialized);

    // for platform theme
    QStringList themeNames = QWlrootsIntegration::instance()->themeNames();

    if (!QGuiApplicationPrivate::platform_theme) {
        for (const QString &themeName : std::as_const(themeNames)) {
            QGuiApplicationPrivate::platform_theme = QPlatformThemeFactory::create(themeName);
            if (QGuiApplicationPrivate::platform_theme) {
                break;
            }
        }
    }

    if (!QGuiApplicationPrivate::platform_theme) {
        for (const QString &themeName : std::as_const(themeNames)) {
            QGuiApplicationPrivate::platform_theme = QWlrootsIntegration::instance()->createPlatformTheme(themeName);
            if (QGuiApplicationPrivate::platform_theme) {
                break;
            }
        }
    }

    if (!QGuiApplicationPrivate::platform_theme) {
        QGuiApplicationPrivate::platform_theme = QWlrootsIntegration::instance()->createPlatformTheme({});
    }

    // fallback
    if (!QGuiApplicationPrivate::platform_theme) {
        QGuiApplicationPrivate::platform_theme = new QPlatformTheme;
    }

    return true;
}

void WServer::start()
{
    W_D(WServer);

    d->init();
}

void WServer::initializeQPA(bool master, const QStringList &parameters)
{
    if (!initializeQtPlatform(master, parameters, nullptr)) {
        qFatal("Can't initialize Qt platform plugin.");
        return;
    }
}

void WServer::initializeProxyQPA(int &argc, char **argv, const QStringList &proxyPlatformPlugins, const QStringList &parameters)
{
    Q_ASSERT(!proxyPlatformPlugins.isEmpty());

    QPlatformIntegration *proxy = nullptr;
    for (const QString &name : std::as_const(proxyPlatformPlugins)) {
        if (name.isEmpty())
            continue;
        proxy = QPlatformIntegrationFactory::create(name, parameters, argc, argv);
        if (proxy)
            break;
    }
    if (!proxy) {
        qFatal() << "Can't create the proxy platform plugin:" << proxyPlatformPlugins;
    }
    proxy->initialize();
    QWlrootsIntegration::instance()->setProxy(proxy);
}

bool WServer::isRunning() const
{
    W_DC(WServer);
    return d->sockNot.get();
}

void WServer::addSocket(WSocket *socket)
{
    W_D(WServer);
    Q_ASSERT(!d->sockets.contains(socket));
    d->sockets.append(socket);

    connect(socket, &WSocket::destroyed, this, [d, socket] {
        d->sockets.removeOne(socket);
    });

    if (d->display)
        d->initSocket(socket);
}

void WServer::setGlobalFilter(GlobalFilterFunc filter, void *data)
{
    W_D(WServer);
    d->globalFilterFunc = filter;
    d->globalFilterFuncData = data;
}

WAYLIB_SERVER_END_NAMESPACE
