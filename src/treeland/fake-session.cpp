// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fake-session.h"

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>
#include <QLocalSocket>
#include <QDBusInterface>
#include <QSocketNotifier>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-server-core.h>

#include <shortcut-client-protocol.h>

struct output {
	struct wl_output *wl_output;
	struct wl_list link;
};

static struct wl_list outputs;
static struct treeland_shortcut_manager_v1 *manager = nullptr;
static struct treeland_shortcut_context_v1 *context = nullptr;

static void registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
    qInfo() << "==== " << interface << treeland_shortcut_manager_v1_interface.name;
	if (strcmp(interface, wl_output_interface.name) == 0) {
		struct output *output = static_cast<struct output*>(calloc(1, sizeof(struct output)));
		output->wl_output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 1);
		wl_list_insert(&outputs, &output->link);
	} else if (strcmp(interface, treeland_shortcut_manager_v1_interface.name) == 0) {
		manager = (treeland_shortcut_manager_v1*)wl_registry_bind(registry, name, &treeland_shortcut_manager_v1_interface, 1);
        context = treeland_shortcut_manager_v1_create(manager);
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

static void context_shortcut(void *data,
                     struct treeland_shortcut_context_v1 *treeland_shortcut_context_v1,
                     uint32_t keycode,
                     uint32_t modify) {
    auto keyEnum = static_cast<Qt::Key>(keycode);
    auto modifyEnum = static_cast<Qt::KeyboardModifiers>(modify);
    qDebug() << keyEnum << modifyEnum;
    if (keyEnum == Qt::Key_Super_L && modifyEnum == Qt::NoModifier) {
        static QProcess process;
        if (process.state() == QProcess::ProcessState::Running) {
            process.kill();
        }
        process.start("dofi", {"-S", "run"});
        return;
    }
    if (keyEnum == Qt::Key_T && modifyEnum.testFlags(Qt::ControlModifier | Qt::AltModifier)) {
        QProcess::startDetached("x-terminal-emulator");
        return;
    }
}

static const struct treeland_shortcut_context_v1_listener shortcut_context_listener {
    .shortcut = context_shortcut,
};

static void sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
    Q_UNUSED(serial)
    bool *done = static_cast<bool *>(data);

    *done = true;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
    sync_callback
};

FakeSession::FakeSession(int argc, char* argv[])
    : QGuiApplication(argc, argv)
{
}

int main (int argc, char *argv[]) {
    wl_list_init(&outputs);
    struct wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        qDebug() << "oh! no!";
        return -1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    wl_display_roundtrip(display);

    Q_ASSERT(manager);
    Q_ASSERT(context);

    treeland_shortcut_context_v1_add_listener(context, &shortcut_context_listener, nullptr);
    treeland_shortcut_context_v1_listen(context);

    auto processWaylandEvents = [display] {
        int ret = 0;
        bool done = false;
        wl_callback *callback = wl_display_sync(display);
        wl_callback_add_listener(callback, &sync_listener, &done);
        if (wl_display_dispatch_pending(display) < 0) {
            return;
        }
        wl_display_flush(display);
        if (QThread::currentThread()->eventDispatcher()) {
            while (!done && ret >= 0) {
                QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
                ret = wl_display_dispatch_pending(display);
            }
        } else {
            while (!done && ret >= 0)
                ret = wl_display_dispatch(display);
        }

        if (ret == -1 && !done)
            wl_callback_destroy(callback);
    };

    FakeSession helper(argc, argv);

    int fd = wl_display_get_fd(display);
    auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
    QObject::connect(notifier, &QSocketNotifier::activated, &helper, [display] {
        if (wl_display_prepare_read(display) == 0) {
            wl_display_read_events(display);
        }
        if (wl_display_dispatch_pending(display) < 0) {
            return;
        }
        wl_display_flush(display);
    });

    processWaylandEvents();

    int exitCode = helper.exec();

    // disconnect from Wayland display
    wl_display_disconnect(display);

    return exitCode;
}
