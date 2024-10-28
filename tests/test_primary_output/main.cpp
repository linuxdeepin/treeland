// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-output-management-v1.h"

#include <QApplication>
#include <QObject>
#include <QWaylandClientExtension>

class PrimaryOutputV1
    : public QWaylandClientExtensionTemplate<PrimaryOutputV1>
    , public QtWayland::treeland_output_manager_v1
{
    Q_OBJECT
public:
    explicit PrimaryOutputV1();

    void treeland_output_manager_v1_primary_output(const QString &output_name)
    {
        qInfo() << "-------Primary Output ----- " << output_name;
    }
};

PrimaryOutputV1::PrimaryOutputV1()
    : QWaylandClientExtensionTemplate<PrimaryOutputV1>(1)
{
}

// 设置主屏: ./test-primary-output HDMI-0
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    PrimaryOutputV1 manager;

    QObject::connect(&manager, &PrimaryOutputV1::activeChanged, &manager, [&manager, argc, argv] {
        if (manager.isActive()) {
            if (argc == 2) {
                const QString str(argv[1]);
                manager.set_primary_output(str);
            }
        }
    });

    return app.exec();
}

#include "main.moc"
