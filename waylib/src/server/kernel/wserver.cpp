// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>

#define private public
#include <QCoreApplication>
#include <private/qhighdpiscaling_p.h>
#undef private

#include "wserver.h"
#include "private/wserver_p.h"
#include "wsurface.h"
#include "platformplugin/qwlrootsintegration.h"

#include <qwdisplay.h>
#include <qwdatadevice.h>

#include <QVector>
#include <QThread>
#include <QEvent>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QMutex>
#include <QDebug>
#include <QProcess>
#include <private/qthread_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformthemefactory_p.h>
#include <qpa/qplatformintegrationfactory_p.h>
#include <qpa/qplatformtheme.h>

extern "C" {
#include <wlr/backend.h>
#define static
#include <wlr/render/wlr_renderer.h>
#undef static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WServerPrivate::WServerPrivate(WServer *qq)
    : WObjectPrivate(qq)
{

}

WServerPrivate::~WServerPrivate()
{
    if (display)
        stop();
}

void WServerPrivate::init()
{
    Q_ASSERT(!display);

    slotOwner.reset(new QObject());
    display = new QWDisplay(q_func());

    // free follow display
    Q_UNUSED(QWDataDeviceManager::create(display));

    W_Q(WServer);

    for (auto i : std::as_const(interfaceList)) {
        i->create(q);
    }

    socket = display->addSocketAuto();
    if (!socket) {
        qFatal("Create socket failed");
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

    Q_EMIT q->started();
}

void WServerPrivate::stop()
{
    W_Q(WServer);

    slotOwner.reset();

    auto i = interfaceList.crbegin();
    for (; i != interfaceList.crend(); ++i) {
        if ((*i)->isValid()) {
            (*i)->destroy(q);
        }

        delete *i;
    }

    interfaceList.clear();
    sockNot.reset();
    QThread::currentThread()->eventDispatcher()->disconnect(q);

    if (display) {
        display->deleteLater();
        display = nullptr;
    }
}

WServer::WServer(QObject *parent)
    : WServer(*new WServerPrivate(this), parent)
{

}

WServer::WServer(WServerPrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{
#ifdef QT_DEBUG
    wlr_log_init(WLR_DEBUG, NULL);
#else
    wlr_log_init(WLR_INFO, NULL);
#endif
}

WDisplayHandle *WServer::handle() const
{
    W_DC(WServer);
    return reinterpret_cast<WDisplayHandle*>(d->display);
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
    d->interfaceList << interface;

    Q_ASSERT(interface->m_server == nullptr);
    interface->m_server = this;

    if (isRunning()) {
        interface->create(this);
    }
}

bool WServer::detach(WServerInterface *interface)
{
    W_D(WServer);
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

QVector<WServerInterface *> WServer::interfaceList() const
{
    W_DC(WServer);
    return d->interfaceList;
}

QVector<WServerInterface *> WServer::findInterfaces(void *handle) const
{
    QVector<WServerInterface*> list;
    Q_FOREACH(auto i, interfaceList()) {
        if (i->handle() == handle)
            list << i;
    }

    return list;
}

WServerInterface *WServer::findInterface(void *handle) const
{
    Q_FOREACH(auto i, interfaceList()) {
        if (i->handle() == handle)
            return i;
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

    W_DC(WServer);
    Q_ASSERT(d->socket);
    qputenv("WAYLAND_DISPLAY", d->socket);
    QPlatformIntegration *proxy = nullptr;
    for (const QString &name : proxyPlatformPlugins) {
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
    qunsetenv("WAYLAND_DISPLAY");
}

void WServer::startProcess(QProcess &process) const
{
    W_DC(WServer);
    Q_ASSERT(d->socket);
    qputenv("WAYLAND_DISPLAY", d->socket);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", d->socket);

    process.setProcessEnvironment(env);
    process.start();
}

bool WServer::isRunning() const
{
    W_DC(WServer);
    return d->display;
}

const char *WServer::displayName() const
{
    if (!isRunning())
        return nullptr;
    W_DC(WServer);
    return d->socket;
}

QObject *WServer::slotOwner() const
{
    W_DC(WServer);
    return d->slotOwner.get();
}

WAYLIB_SERVER_END_NAMESPACE
