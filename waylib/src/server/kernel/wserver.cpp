/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#define private public
#include <QCoreApplication>
#undef private

#include "wserver.h"
#include "wsurface.h"
#include "wthreadutils.h"
#include "wseat.h"

#include <QVector>
#include <QThread>
#include <QEvent>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QMutex>
#include <QDebug>
#include <private/qthread_p.h>

extern "C" {
#define static
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServerPrivate;
class ServerThread : public QDaemonThread
{
    Q_OBJECT
public:
    ServerThread(WServerPrivate *s)
        : server(s) {}

    bool event(QEvent *e) override;
    Q_SLOT void processWaylandEvents();

private:
    WServerPrivate *server;
    static QEvent::Type callFunctionEventType;
    friend class WServer;
};
QEvent::Type ServerThread::callFunctionEventType
    = static_cast<QEvent::Type>(QEvent::registerEventType());

class WServerPrivate : public WObjectPrivate
{
public:
    WServerPrivate(WServer *qq)
        : WObjectPrivate(qq)
    {
        // Mark to uninitialized
        initialized.lock();
    }
    ~WServerPrivate()
    {
        Q_ASSERT_X(!thread || !thread->isRunning(), "WServer",
                   "Must stop the server before destroy it.");
        qDeleteAll(interfaceList);
    }

    void init();
    void stop();

    W_DECLARE_PUBLIC(WServer)
    QScopedPointer<ServerThread> thread;
    QScopedPointer<QSocketNotifier> sockNot;
    QScopedPointer<WThreadUtil> threadUtil;
    QMutex initialized;

    QVector<WServerInterface*> interfaceList;

    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
};

void WServerPrivate::init()
{
    Q_ASSERT(!display);

    display = wl_display_create();

    // free follow display
    wlr_data_device_manager_create(display);

    W_Q(WServer);

    for (auto i : qAsConst(interfaceList)) {
        i->create(q);
    }

    const char *socket = wl_display_add_socket_auto(display);

    if (!socket) {
        qFatal("Create socket failed");
    }

    loop = wl_display_get_event_loop(display);
    int fd = wl_event_loop_get_fd(loop);

    sockNot.reset(new QSocketNotifier(fd, QSocketNotifier::Read));
    QObject::connect(sockNot.get(), &QSocketNotifier::activated,
                     thread.get(), &ServerThread::processWaylandEvents);

    QAbstractEventDispatcher *dispatcher = thread->eventDispatcher();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock,
                     thread.get(), &ServerThread::processWaylandEvents);

    // Mark to initialized
    initialized.unlock();

    QMetaObject::invokeMethod(q, &WServer::started, Qt::QueuedConnection);
}

void WServerPrivate::stop()
{
    W_Q(WServer);
    auto i = interfaceList.crbegin();
    for (; i != interfaceList.crend(); ++i) {
        if ((*i)->isValid()) {
            (*i)->destroy(q);
        }

        delete *i;
    }

    interfaceList.clear();
    sockNot.reset();
    thread->eventDispatcher()->disconnect(thread.get());

    if (display) {
        wl_display_destroy(display);
    }

    // delete children in server thread
    QObjectPrivate::get(q)->deleteChildren();
}

bool ServerThread::event(QEvent *e)
{
    if (e->type() == callFunctionEventType) {
        auto ev = static_cast<WThreadUtil::CallEvent*>(e);
        if (Q_LIKELY(ev->target))
            ev->function();
        return true;
    }
    return ServerThread::event(e);
}

void ServerThread::processWaylandEvents()
{
    int ret = wl_event_loop_dispatch(server->loop, 0);
    if (ret)
        fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
    wl_display_flush_clients(server->display);
}

WServer::WServer()
    : QObject()
    , WObject(*new WServerPrivate(this))
{
#ifdef QT_DEBUG
//    wlr_log_init(WLR_DEBUG, NULL);
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

    Q_ASSERT(d->thread && d->display);
    Q_ASSERT(d->thread->isRunning());
    Q_ASSERT(QThread::currentThread() != d->thread.data());

//    DThreadUtil::runInThread(d->thread.get(), this,
//                             d, &WServerPrivate::stop);

    d->thread->quit();
    if (!d->thread->wait()) {
        d->thread->terminate();
    }
}

void WServer::attach(WServerInterface *interface)
{
    W_D(WServer);
    Q_ASSERT(!d->interfaceList.contains(interface));
    d->interfaceList << interface;

    if (isRunning()) {
        d->threadUtil->run(this, interface, &WServerInterface::create, this);
    }
}

bool WServer::detach(WServerInterface *interface)
{
    W_D(WServer);
    if (!d->interfaceList.contains(interface))
        return false;

    if (!isRunning())
        return false;

    d->threadUtil->run(this, interface, &WServerInterface::destroy, this);
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

WServer *WServer::fromThread(const QThread *thread)
{
    if (auto st = qobject_cast<const ServerThread*>(thread)) {
        return st->server->q_func();
    }

    return nullptr;
}

void WServer::start()
{
    W_D(WServer);

    if (d->thread && d->thread->isRunning())
        return;

    if (!d->thread) {
        d->thread.reset(new ServerThread(d));
        d->thread->moveToThread(d->thread.get());
        d->threadUtil.reset(new WThreadUtil(d->thread.get(),
                                             ServerThread::callFunctionEventType));
        moveToThread(d->thread.get());
        Q_ASSERT(thread() == d->thread.get());
    }

    d->thread->start(QThread::HighestPriority);
    d->threadUtil->run(this, d, &WServerPrivate::init);
}

static inline bool tryLock(QMutex *mutex, int timeout)
{
    bool ok = mutex->tryLock(timeout);
    if (ok)
        mutex->unlock();
    return ok;
}

bool WServer::waitForStarted(int timeout)
{
    if (isRunning())
        return true;

    W_D(WServer);
    return tryLock(&d->initialized, timeout);
}

bool WServer::waitForStoped(int timeout)
{
    if (!isRunning())
        return true;

    W_D(WServer);
    return d->thread->wait(timeout);
}

bool WServer::isRunning() const
{
    W_DC(WServer);
    return tryLock(&const_cast<WServerPrivate*>(d)->initialized, 0);
}

WThreadUtil *WServer::threadUtil() const
{
    W_DC(WServer);
    return d->threadUtil.get();
}

bool WServer::event(QEvent *e)
{
    if (e->type() != WInputEvent::type())
        return QObject::event(e);

    e->ignore();
    return true;
}

WAYLIB_SERVER_END_NAMESPACE

#include "wserver.moc"
