// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QObject>
#include <QCoreApplication>
#include <private/qhighdpiscaling_p.h>
#include "private/wprivateaccessor_p.h"

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
#include <QScopedValueRollback>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <unistd.h>
#include <private/qthread_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformthemefactory_p.h>
#include <qpa/qplatformtheme.h>

QW_USE_NAMESPACE
W_DECLARE_PRIVATE_STATIC_MEMBER(QHighDpiScaling_m_globalScalingActive_tag, QHighDpiScaling, m_globalScalingActive, bool);
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

    sockNot.reset(new QSocketNotifier(fd, QSocketNotifier::Read));
    bool ok = QObject::connect(sockNot.get(), SIGNAL(activated(QSocketDescriptor,QSocketNotifier::Type)),
                               q, SLOT(processWaylandEvents()));
    Q_ASSERT(ok);

    // Match upstream wl_display_run order: flush before dispatch.
    QAbstractEventDispatcher *dispatcher = QThread::currentThread()->eventDispatcher();
    ok = QObject::connect(dispatcher, SIGNAL(aboutToBlock()), q, SLOT(onAboutToBlock()));
    Q_ASSERT(ok);
    ok = QObject::connect(dispatcher, SIGNAL(awake()), q, SLOT(onAwake()));
    Q_ASSERT(ok);

    for (auto socket : std::as_const(sockets))
        initSocket(socket);

    Q_EMIT q->started();
}

void WServerPrivate::stop()
{
    W_Q(WServer);

    // Disconnect event handlers BEFORE destroying clients to prevent
    // callbacks from firing during client destruction.
    sockNot.reset();
    if (auto *dispatcher = QThread::currentThread()->eventDispatcher())
        QObject::disconnect(dispatcher, nullptr, q, nullptr);

    if (display)
        wl_display_destroy_clients(*display);

    auto list = interfaceList;
    interfaceList.clear();
    auto i = list.crbegin();
    for (; i != list.crend(); ++i) {
        (*i)->destroy(q);
        delete *i;
    }
}

// Replace wl_display_flush_clients: its wl_list_for_each_safe loop deadlocks
// when destroy-signal cascades make the pre-saved next node self-linked.
void WServerPrivate::safeFlushClients()
{
    struct wl_list *head = wl_display_get_client_list(display->handle());
    struct wl_list *node = head->next;
    while (node != head) {
        // Self-linked node: already destroyed, stop to avoid infinite loop.
        if (node->next == node)
            break;
        struct wl_list *next = node->next;
        wl_client_flush(wl_client_from_link(node));
        node = next;
    }
}

void WServerPrivate::initSocket(WSocket *socketServer)
{
    bool ok = socketServer->listen(display->handle());
    Q_ASSERT(ok);
}

void WServerPrivate::processWaylandEvents()
{
    if (isProcessingEvents)
        return;

    QScopedValueRollback<bool> guard(isProcessingEvents, true);

    int ret = wl_event_loop_dispatch(loop, 0);
    if (ret)
        fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
}

/*
 * NOTE: Wayland idle dispatch integration (Qt + wlroots hybrid loop)
 *
 * In a native wlroots/libwayland architecture, wl_event_loop_dispatch()
 * is continuously driven by wl_display_run(), which guarantees a strict
 * event-loop ordering:
 *
 *   1. poll()/epoll_wait() for fd activity
 *   2. dispatch fd/timer/signal sources
 *   3. dispatch idle sources (wl_event_loop_dispatch_idle)
 *   4. return immediately into the next loop iteration
 *
 * In that model, idle callbacks are always executed within the same
 * dispatch cycle in which they become pending, and newly scheduled idle
 * tasks (e.g. from wlroots such as wlr_output_schedule_frame()) are
 * guaranteed to be picked up in a timely next iteration.
 *
 * ---------------------------------------------------------------------
 * Qt integration problem
 * ---------------------------------------------------------------------
 *
 * In this architecture, Qt owns the main event loop, and Wayland is
 * driven manually via QSocketNotifier on wl_display_get_fd().
 *
 * This changes the execution model fundamentally:
 *
 *   Qt Event Loop:
 *     -> processes Qt events
 *     -> enters aboutToBlock()
 *     -> waits (epoll/poll)
 *
 *   Wayland:
 *     -> only progresses when Qt triggers wl_event_loop_dispatch()
 *        due to fd readability
 *
 * Therefore, wl_event_loop_dispatch(loop, 0) is NOT continuously running
 * like wl_display_run(), but only executed opportunistically.
 *
 * ---------------------------------------------------------------------
 * The critical correctness issue (idle starvation)
 * ---------------------------------------------------------------------
 *
 * When wl_event_loop_dispatch(loop, 0) is executed:
 *
 *   - all currently pending Wayland fd/timer/signal events are processed
 *   - wl_event_loop_dispatch_idle(loop) runs
 *   - at this point, ALL idle callbacks existing at that moment are executed
 *
 * However, after this point, control returns to Qt immediately.
 *
 * During subsequent Qt event processing, it is possible that:
 *
 *   - Qt handlers trigger wlroots logic
 *   - wlroots schedules deferred work via wl_event_loop_add_idle()
 *     (e.g. wlr_output_schedule_frame, commit batching, repaint deferral)
 *
 * These newly added idle callbacks are NOT associated with any fd activity.
 *
 * As a result:
 *
 *   - wl_display fd remains inactive
 *   - QSocketNotifier is not triggered
 *   - wl_event_loop_dispatch(loop, 0) is not called again
 *   - idle callbacks remain pending indefinitely
 *
 * They will only be executed later when some unrelated Wayland fd event
 * occurs, causing wl_event_loop_dispatch() to run again — by which time
 * the intended scheduling point (frame timing / batching window) is already
 * delayed and incorrect relative to wlroots expectations.
 *
 * ---------------------------------------------------------------------
 * Why aboutToBlock fixes this
 * ---------------------------------------------------------------------
 *
 * QAbstractEventDispatcher::aboutToBlock() is emitted at a key semantic
 * boundary in Qt:
 *
 *   - all Qt events have been processed
 *   - no more Qt event handlers are running
 *   - the event loop is about to enter a blocking wait
 *
 * This makes it equivalent to the "end of event-loop iteration" point.
 *
 * By invoking wl_event_loop_dispatch_idle(loop) here, we effectively:
 *
 *   - emulate wl_display_run() idle phase
 *   - ensure all pending Wayland idle callbacks are executed
 *   - drain idle tasks that were scheduled during Qt event processing
 *
 * This also covers the second critical case:
 *
 *   Qt wake-up cycle:
 *     Qt processes events
 *     -> wlroots schedules new idle work
 *     -> Qt may not hit a new Wayland fd event immediately
 *     -> idle would otherwise be delayed
 *
 * Therefore, we must also process idle:
 *
 *   - right before Qt sleeps (aboutToBlock)
 *   - and effectively on each Qt iteration boundary
 *
 * to preserve wl_display_run() semantics.
 *
 * ---------------------------------------------------------------------
 * Summary
 * ---------------------------------------------------------------------
 *
 * This manual call to wl_event_loop_dispatch_idle(loop) is required to
 * restore the implicit guarantees provided by wl_display_run():
 *
 *   - idle callbacks are executed within the same logical iteration
 *     in which they are scheduled
 *   - wlroots frame scheduling (e.g. wlr_output_schedule_frame) is not
 *     delayed until the next unrelated Wayland fd event
 *
 * Without this, idle work can be starved indefinitely in a Qt-driven
 * event loop where no Wayland fd activity occurs.
 *
 * This is a deliberate synchronization point between Qt's event loop
 * lifecycle and Wayland's idle scheduling model.
 */
void WServerPrivate::onAboutToBlock()
{
    if (isProcessingEvents)
        return;

    wl_event_loop_dispatch_idle(loop);
    safeFlushClients();
}

void WServerPrivate::onAwake()
{
    if (isProcessingEvents)
        return;

    wl_event_loop_dispatch_idle(loop);
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

static bool initializeQtPlatform(const QStringList &parameters, std::function<void()> onInitialized)
{
    Q_ASSERT(QGuiApplication::instance() == nullptr);
    if (QGuiApplicationPrivate::platform_integration)
        return false;

    QHighDpiScaling::initHighDpiScaling();
    W_PRIVATE_STATIC_MEMBER(QHighDpiScaling_m_globalScalingActive_tag{}) = true; // force enable hidpi
    QGuiApplicationPrivate::platform_integration = new QWlrootsIntegration(parameters, onInitialized);

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

void WServer::initializeQPA(const QStringList &parameters,
                            std::function<QPlatformTheme *(const QString &)> createPlatformTheme)
{
    if (createPlatformTheme) {
        QWlrootsIntegration::setCreatePlatformThemeCallback(std::move(createPlatformTheme));
    }

    if (!initializeQtPlatform(parameters, nullptr)) {
        qFatal("Can't initialize Qt platform plugin.");
        return;
    }
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
