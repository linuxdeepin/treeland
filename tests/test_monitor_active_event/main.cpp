// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-dde-shell-v1.h"

#include <QGuiApplication>
#include <QObject>
#include <QWaylandClientExtension>

#define TREELANDDDESHELLMANAGERV1VERSION 1

class TreelandDDEShellManageV1
    : public QWaylandClientExtensionTemplate<TreelandDDEShellManageV1>
    , public QtWayland::treeland_dde_shell_manager_v1
{
    Q_OBJECT
public:
    TreelandDDEShellManageV1();
    ~TreelandDDEShellManageV1() = default;

    void instantiate();
};

class TreelandDDEActiveV1
    : public QObject
    , public QtWayland::treeland_dde_active_v1
{
    Q_OBJECT
public:
    TreelandDDEActiveV1(struct ::treeland_dde_active_v1 *id);
    ~TreelandDDEActiveV1();

protected:
    void treeland_dde_active_v1_active_in(uint32_t reason) override;
    void treeland_dde_active_v1_active_out(uint32_t reason) override;
    void treeland_dde_active_v1_start_drag() override;
};

TreelandDDEShellManageV1::TreelandDDEShellManageV1()
    : QWaylandClientExtensionTemplate<TreelandDDEShellManageV1>(TREELANDDDESHELLMANAGERV1VERSION)
    , QtWayland::treeland_dde_shell_manager_v1()
{
}

void TreelandDDEShellManageV1::instantiate()
{
    initialize();
}

TreelandDDEActiveV1::TreelandDDEActiveV1(struct ::treeland_dde_active_v1 *id)
    : QtWayland::treeland_dde_active_v1(id)
{
}

TreelandDDEActiveV1::~TreelandDDEActiveV1()
{
    destroyed();
}

void TreelandDDEActiveV1::treeland_dde_active_v1_active_in(uint32_t reason)
{
    qWarning() << "recvie button activeIn, reson:" << reason;
}

void TreelandDDEActiveV1::treeland_dde_active_v1_active_out(uint32_t reason)
{
    qWarning() << "recvie button activeOut, reson:" << reason;
}

void TreelandDDEActiveV1::treeland_dde_active_v1_start_drag()
{
    qWarning() << "------------start_drag";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    TreelandDDEShellManageV1 *manager = new TreelandDDEShellManageV1;
    QObject::connect(manager, &TreelandDDEShellManageV1::activeChanged, [manager] {
        if (manager->isActive()) {
            auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            if (!waylandApp) {
                return;
            }
            auto seat = waylandApp->seat();

            if (!seat)
                qFatal("Failed to get wl_seat from QtWayland QPA!");

            TreelandDDEActiveV1 *active =
                new TreelandDDEActiveV1(manager->get_treeland_dde_active(seat));
        }
    });

    manager->instantiate();

    return app.exec();
}

#include "main.moc"
