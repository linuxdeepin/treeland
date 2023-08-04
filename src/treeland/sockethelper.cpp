#include "sockethelper.h"

#include "SocketWriter.h"
#include "Messages.h"

#include <QDebug>
#include <QObject>

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>
#include <QLocalSocket>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-server-core.h>
#include <wsocket.h>

#include <client-protocol.h>

using namespace SDDM;

WAYLIB_SERVER_USE_NAMESPACE

struct output {
	struct wl_output *wl_output;
	struct wl_list link;
};

static struct wl_list outputs;
static struct treeland_socket_manager_v1 *manager = nullptr;

static void registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
    qInfo() << "==== " << interface << treeland_socket_manager_v1_interface.name;
	if (strcmp(interface, wl_output_interface.name) == 0) {
		struct output *output = static_cast<struct output*>(calloc(1, sizeof(struct output)));
		output->wl_output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 1);
		wl_list_insert(&outputs, &output->link);
	} else if (strcmp(interface, treeland_socket_manager_v1_interface.name) == 0) {
		manager = (treeland_socket_manager_v1*)wl_registry_bind(registry, name, &treeland_socket_manager_v1_interface, 1);
	}
}

static void registry_handle_global_remove(void *data,
		struct wl_registry *registry, uint32_t name) {
	// Who cares?
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

SocketHelper::SocketHelper(int argc, char* argv[])
    : QGuiApplication(argc, argv)
{
    const QStringList args = QCoreApplication::arguments();
    QString server;
    int pos;

    if ((pos = args.indexOf(QStringLiteral("--socket"))) >= 0) {
        if (pos >= args.length() - 1) {
            qCritical() << "This application is not supposed to be executed manually";
            return;
        }
        server = args[pos + 1];
    }

    m_socket = new QLocalSocket;

    connect(m_socket, &QLocalSocket::connected, this, &SocketHelper::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &SocketHelper::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &SocketHelper::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &SocketHelper::error);

    m_socket->connectToServer(server);
}

void SocketHelper::connected() {
    qDebug() << "[treeland helper] Connected to the daemon.";
    SocketWriter(m_socket) << quint32(TreelandMessages::Connect);
}

void SocketHelper::disconnected() {
    qDebug() << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qDebug() << "Display Manager is closed socket connect, quiting treeland.";
    exit();
}

void SocketHelper::readyRead() {
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DaemonMessages(message)) {
            case DaemonMessages::CreateWaylandSocket: {
                QString username;
                input >> username;

                qDebug() << username;

                WSocket *socket = new WSocket(true);
                struct passwd *pw = getpwnam(username.toUtf8());
                socket->autoCreate(QString("/run/user/%1").arg(pw->pw_uid));

                qDebug() << "chown wayland socket: " << chown(socket->fullServerName().toUtf8(), pw->pw_uid, pw->pw_gid);

                qDebug() << "create user wayland socket: " << username;
                qDebug() << "create user wayland socket fd: " << socket->socketFd();

                auto context_inter = treeland_socket_manager_v1_create(manager);
                treeland_socket_context_v1_set_username(context_inter, username.toUtf8());
                treeland_socket_context_v1_set_fd(context_inter, socket->socketFd());
                treeland_socket_context_v1_commit(context_inter);

                SocketWriter(m_socket) << quint32(TreelandMessages::NewWaylandSocket) << username << socket->fullServerName();
            }
            break;
            default:
            break;
        }
    }
}

void SocketHelper::error() {
    qCritical() << "Socket error: " << m_socket->errorString();
}

// TODO: 接受 socket，从环境变量中连接上 WAYLAND，走 wayland 协议为用户创建 socket。
int main (int argc, char *argv[]) {
    Q_ASSERT(::getuid() == 0);
    if (argc != 3) {
        QTextStream(stderr) << "Wrong number of arguments\n";
        return -1;
    }

	wl_list_init(&outputs);
    struct wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        qDebug() << "oh! no!";
        return -1;
    }

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

    Q_ASSERT(manager);

    auto processWaylandEvents = [display] {
        wl_display_dispatch_pending(display);
        wl_display_flush(display);
    };

    SocketHelper helper(argc, argv);

    QObject::connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, qApp, processWaylandEvents);
    QObject::connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::awake, qApp, processWaylandEvents);

    return helper.exec();
}
