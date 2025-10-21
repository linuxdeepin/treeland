// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QApplication>
#include <QObject>
#include <QScreen>
#include <QWaylandClientExtension>

#include "qwayland-treeland-output-manager-v1.h"


static wl_output *wlOutputForName(const QString &name)
{
    auto pni = QGuiApplication::platformNativeInterface();
    if (!pni) {
        qWarning() << "Not running on a platform with a native interface (Wayland?).";
        return nullptr;
    }

    for (QScreen *screen : QGuiApplication::screens()) {
        const QString sname = screen->name(); // from xdg-output name on Wayland
        if (QString::compare(sname, name, Qt::CaseInsensitive) == 0) {
            auto *res = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
            return res->output();
        }
    }
    return nullptr;
}

class OutputManagerV1
    : public QWaylandClientExtensionTemplate<OutputManagerV1>
    , public QtWayland::treeland_output_manager_v1
{
    Q_OBJECT
public:
    explicit OutputManagerV1();
};

OutputManagerV1::OutputManagerV1()
    : QWaylandClientExtensionTemplate<OutputManagerV1>(1)
{
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    OutputManagerV1 manager;
    QObject::connect(&manager, &OutputManagerV1::activeChanged, &manager, [&manager, argc, argv, &app] {
        if (manager.isActive()) {
            auto output = wlOutputForName(argv[1]);
            treeland_output_color_control_v1 *native_control = nullptr;
            QtWayland::treeland_output_color_control_v1 *colorControl = nullptr;
            if (output) {
                native_control = manager.get_color_control(output);
                colorControl = new QtWayland::treeland_output_color_control_v1(native_control);
                printf("output: %p\n", output);
                printf("native_control: %p\n", native_control);
                printf("colorControl: %p\n", &colorControl);
                    colorControl->set_color_temperature(atoi(argv[2]));
                    colorControl->set_brightness(atoi(argv[3]));
            }
            if (colorControl)
                delete colorControl;
            app.quit();
        }
    });
    app.exec();

}

#include "main.moc"
