// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsocket.h"

#include <QDir>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QPointer>

#include <wayland-server-core.h>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

struct wl_event_source;

WAYLIB_SERVER_BEGIN_NAMESPACE

#define LOCK_SUFFIX ".lock"

// Copy from libwayland
static int wl_socket_lock(const QString &socketFile)
{
    int fd_lock = -1;
    struct stat socket_stat;

    QString lockFile = socketFile + LOCK_SUFFIX;
    QByteArray addr_sun_path = socketFile.toUtf8();

    fd_lock = open(lockFile.toUtf8(), O_CREAT | O_CLOEXEC | O_RDWR,
                   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

    if (fd_lock < 0) {
        qDebug("unable to open lockfile %s check permissions", qPrintable(lockFile));
        goto err;
    }

    if (flock(fd_lock, LOCK_EX | LOCK_NB) < 0) {
        qDebug("unable to lock lockfile %s, maybe another compositor is running", qPrintable(lockFile));
        goto err_fd;
    }

    if (lstat(addr_sun_path, &socket_stat) < 0 ) {
        if (errno != ENOENT) {
            qDebug("did not manage to stat file %s\n", qPrintable(addr_sun_path));
            goto err_fd;
        }
    } else if (socket_stat.st_mode & S_IWUSR ||
               socket_stat.st_mode & S_IWGRP) {
        unlink(addr_sun_path);
    }

    return fd_lock;
err_fd:
    close(fd_lock);
    fd_lock = -1;
err:
    return fd_lock;
}

static int set_cloexec_or_close(int fd)
{
    long flags;

    if (fd == -1)
        return -1;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1)
        goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        goto err;

    return fd;

err:
    close(fd);
    return -1;
}

static int wl_os_socket_cloexec(int domain, int type, int protocol)
{
    int fd;

    fd = socket(domain, type | SOCK_CLOEXEC, protocol);
    if (fd >= 0)
        return fd;
    if (errno != EINVAL)
        return -1;

    fd = socket(domain, type, protocol);
    return set_cloexec_or_close(fd);
}

static int wl_os_accept_cloexec(int sockfd, sockaddr *addr, socklen_t *addrlen)
{
    int fd = accept(sockfd, addr, addrlen);
    return set_cloexec_or_close(fd);
}
// Copy end

class WSocketPrivate : public WObjectPrivate
{
public:
    WSocketPrivate(WSocket *qq, bool freeze, WSocket *parent)
        : WObjectPrivate(qq)
        , freezeClientWhenDisable(freeze)
        , parentSocket(parent)
    {}

    ~WSocketPrivate() {
        q_func()->close();
    }

    static WSocketPrivate *get(WSocket *qq) {
        return qq->d_func();
    }

    void shutdown();
    void restore();

    void addClient(wl_client *client);
    wl_client *createClient(int fd);
    void removeClient(wl_client *client);
    void destroyClient(wl_client *client);

    W_DECLARE_PUBLIC(WSocket)

    bool enabled = true;
    const bool freezeClientWhenDisable;
    int fd = -1;
    int fd_lock = -1;
    QString socket_file;
    const QPointer<WSocket> parentSocket;

    wl_display *display = nullptr;
    wl_event_source *eventSource = nullptr;
    QList<wl_client*> clients;
};


struct WlClientDestroyListener {
    WlClientDestroyListener(WSocket *socket)
        : socket(socket)
    {
        destroy.notify = handle_destroy;
    }

    ~WlClientDestroyListener();

    static WlClientDestroyListener *get(const wl_client *client);
    static void handle_destroy(struct wl_listener *listener, void *);

    wl_listener destroy;
    QPointer<WSocket> socket;
};

WlClientDestroyListener::~WlClientDestroyListener()
{
    wl_list_remove(&destroy.link);
}

WlClientDestroyListener *WlClientDestroyListener::get(const wl_client *client)
{
    wl_listener *listener = wl_client_get_destroy_listener(const_cast<wl_client*>(client),
                                                           WlClientDestroyListener::handle_destroy);
    if (!listener) {
        return nullptr;
    }

    WlClientDestroyListener *tmp = wl_container_of(listener, tmp, destroy);
    return tmp;
}

void WlClientDestroyListener::handle_destroy(wl_listener *listener, void *data)
{
    WlClientDestroyListener *self = wl_container_of(listener, self, destroy);
    if (self->socket)
        WSocketPrivate::get(self->socket)->removeClient(reinterpret_cast<wl_client*>(data));
    delete self;
}

static bool pauseClient(wl_client *client, bool pause)
{
    pid_t pid = 0;
    wl_client_get_credentials(client, &pid, nullptr, nullptr);

    if (pid == 0)
        return false;

    return kill(pid, pause ? SIGSTOP : SIGCONT) == 0;
}

void WSocketPrivate::shutdown()
{
    if (!freezeClientWhenDisable)
        return;

    for (auto client : clients) {
        pauseClient(client, true);
    }
}

void WSocketPrivate::restore()
{
    if (!freezeClientWhenDisable)
        return;

    for (auto client : clients) {
        pauseClient(client, false);
    }
}

void WSocketPrivate::addClient(wl_client *client)
{
    Q_ASSERT(!clients.contains(client));
    clients.append(client);

    auto listener = new WlClientDestroyListener(q_func());
    wl_client_add_destroy_listener(client, &listener->destroy);

    if (!enabled && freezeClientWhenDisable) {
        pauseClient(client, true);
    }

    Q_EMIT q_func()->clientsChanged();
}

wl_client *WSocketPrivate::createClient(int fd)
{
    auto wl_client = wl_client_create(display, fd);
    if (!wl_client) {
        qWarning() << "wl_client_create failed";
    }

    return wl_client;
}

void WSocketPrivate::removeClient(wl_client *client)
{
    bool ok = clients.removeOne(client);
    if (ok) {
        destroyClient(client);
        Q_EMIT q_func()->clientsChanged();
    }
}

void WSocketPrivate::destroyClient(wl_client *client)
{
    auto listener = WlClientDestroyListener::get(client);
    delete listener;
}

WSocket::WSocket(bool freezeClientWhenDisable, WSocket *parentSocket, QObject *parent)
    : QObject(parent)
    , WObject(*new WSocketPrivate(this, freezeClientWhenDisable, parentSocket))
{

}

WSocket *WSocket::get(const wl_client *client)
{
    if (auto tmp = WlClientDestroyListener::get(client))
        return tmp->socket;
    return nullptr;
}

WSocket *WSocket::parentSocket() const
{
    W_DC(WSocket);
    return d->parentSocket.get();
}

WSocket *WSocket::rootSocket() const
{
    W_DC(WSocket);
    return d->parentSocket ? d->parentSocket->rootSocket() : const_cast<WSocket*>(this);
}

bool WSocket::isValid() const
{
    W_DC(WSocket);
    return d->fd >= 0;
}

void WSocket::close()
{
    W_D(WSocket);

    if (d->eventSource) {
        wl_event_source_remove(d->eventSource);
        d->eventSource = nullptr;
        d->display = nullptr;
        Q_EMIT listeningChanged();
    }
    Q_ASSERT(!d->display);

    if (d->fd >= 0) {
        ::close(d->fd);
        d->fd = -1;
        Q_EMIT validChanged();
    }
    if (d->fd_lock >= 0) {
        ::close(d->fd_lock);
        d->fd_lock = -1;
    }

    if (!d->clients.isEmpty()) {
        for (auto client : d->clients)
            d->destroyClient(client);

        d->clients.clear();
        Q_EMIT clientsChanged();
    }
}

SOCKET WSocket::socketFd() const
{
    W_DC(WSocket);
    return d->fd;
}

QString WSocket::fullServerName() const
{
    W_DC(WSocket);
    return d->socket_file;
}

bool WSocket::autoCreate(const QString &directory)
{
    // A reasonable number of maximum default sockets. If
    // you need more than this, use other API.
    constexpr int MAX_DISPLAYNO = 32;

    QString dir;

    if (directory.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (dir.isEmpty() || dir == QDir::rootPath())
            return false;
    } else {
        dir = directory;
    }

    for (int i = 0; i < MAX_DISPLAYNO; ++i) {
        if (create(QString("%1/wayland-%2").arg(dir).arg(i)))
            return true;
    }

    return false;
}

bool WSocket::create(const QString &filePath)
{
    W_D(WSocket);

    if (isValid())
        return false;

    d->fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
    if (d->fd < 0)
        return false;

    d->fd_lock = wl_socket_lock(filePath);
    if (d->fd_lock < 0) {
        close();
        return false;
    }

    QByteArray path = filePath.toUtf8();
    sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    const size_t pathLength = qMin(size_t(sizeof(addr.sun_path)), size_t(path.size()) + 1);
    qstrncpy(addr.sun_path, path.constData(), pathLength);
    socklen_t size = offsetof(sockaddr_un, sun_path) + pathLength;
    if (::bind(d->fd, (sockaddr*) &addr, size) < 0) {
        close();
        qDebug("bind() failed with error: %s", strerror(errno));
        return false;
    }

    if (::listen(d->fd, 128) < 0) {
        close();
        qDebug("listen() failed with error: %s\n", strerror(errno));
        return false;
    }

    if (d->socket_file != filePath) {
        d->socket_file = filePath;
        Q_EMIT fullServerNameChanged();
    }

    Q_EMIT validChanged();

    return true;
}

bool WSocket::create(int fd, bool doListen)
{
    W_D(WSocket);

    if (isValid())
        return false;

    {   // check socket file
        struct ::stat stat_buf = {0};
        if (fstat(fd, &stat_buf) != 0) {
            qDebug("fstat failed on create by FD");
            return false;
        } else if (!S_ISSOCK(stat_buf.st_mode)) {
            qDebug("fd is not a socket");
            return false;
        }

        int accept_conn = 0;
        socklen_t accept_conn_size = sizeof(accept_conn);
        if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &accept_conn,
                       &accept_conn_size) != 0) {
            qDebug("getsockopt failed on FD");
            return false;
        } else if (accept_conn == 0) {
            qDebug("fd is not a socket");
            return false;
        }
    }

    QString socketFile;

    {   // get socket file path
        struct ::sockaddr_un addr;
        socklen_t len = sizeof(addr);
        memset(&addr, 0, sizeof(addr));
        const int getpeernameStatus = ::getpeername(fd, (sockaddr *)&addr, &len);
        if (getpeernameStatus != 0 || len == offsetof(sockaddr_un, sun_path)) {
            // this is the case when we call it from QLocalServer, then there is no peername
            len = sizeof(addr);
            if (::getsockname(fd, (sockaddr *)&addr, &len) != 0)
                return false;
        }

        if (len <= offsetof(::sockaddr_un, sun_path))
            return false;
        len -= offsetof(::sockaddr_un, sun_path);

        QStringDecoder toUtf16(QStringDecoder::System, QStringDecoder::Flag::Stateless);
        QByteArrayView textData(addr.sun_path, len);
        QString name = toUtf16(textData);
        if (!name.isEmpty() && !toUtf16.hasError()) {
            //conversion encodes the trailing zeros. So, in case of non-abstract namespace we
            //chop them off as \0 character is not allowed in filenames
            if ((name.at(name.size() - 1) == QChar::fromLatin1('\0'))) {
                int truncPos = name.size() - 1;
                while (truncPos > 0 && name.at(truncPos - 1) == QChar::fromLatin1('\0'))
                    truncPos--;
                name.truncate(truncPos);
            }

            socketFile = name;
        } else {
            return false;
        }
    }

    if (doListen && ::listen(d->fd, 128) < 0) {
        qDebug("listen() failed with error: %s\n", strerror(errno));
        return false;
    }

    d->fd = fd;

    if (d->socket_file != socketFile) {
        d->socket_file = socketFile;
        Q_EMIT fullServerNameChanged();
    }

    return true;
}

bool WSocket::isListening() const
{
    W_DC(WSocket);
    return d->eventSource;
}

static int socket_data(int fd, uint32_t, void *data)
{
    WSocketPrivate *d = reinterpret_cast<WSocketPrivate*>(data);
    sockaddr_un name;
    socklen_t length;
    int client_fd;

    length = sizeof name;
    client_fd = wl_os_accept_cloexec(fd, (sockaddr*)&name, &length);
    if (client_fd < 0) {
        qDebug("failed to accept: %s", strerror(errno));
    } else {
        auto client = d->createClient(client_fd);
        if (client)
            d->addClient(client);
    }

    return 1;
}

bool WSocket::listen(wl_display *display)
{
    W_D(WSocket);

    if (d->eventSource || !isValid())
        return false;

    auto loop = wl_display_get_event_loop(display);
    if (!loop)
        return false;

    d->display = display;
    d->eventSource = wl_event_loop_add_fd(loop, d->fd,
                                          WL_EVENT_READABLE,
                                          socket_data, d);
    if (!d->eventSource)
        return false;

    Q_EMIT listeningChanged();

    return true;
}

void WSocket::addClient(wl_client *client)
{
    W_D(WSocket);
    if (d->clients.contains(client))
        return;
    d->addClient(client);
}

void WSocket::removeClient(wl_client *client)
{
    W_D(WSocket);
    d->removeClient(client);
}

QList<wl_client*> WSocket::clients() const
{
    W_DC(WSocket);
    return d->clients;
}

bool WSocket::isEnabled() const
{
    W_DC(WSocket);
    return d->enabled;
}

void WSocket::setEnabled(bool on)
{
    W_D(WSocket);
    if (d->enabled == on)
        return;
    d->enabled = on;

    if (d->enabled) {
        d->restore();
    } else {
        d->shutdown();
    }

    Q_EMIT enabledChanged();
}

WAYLIB_SERVER_END_NAMESPACE
