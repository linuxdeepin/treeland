// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-virtual-output-manager-v1.h"

#include <private/qwaylandscreen_p.h>
#include <private/qwaylandwindow_p.h>

#include <qwindow.h>

#include <QApplication>
#include <QPushButton>
#include <QtWaylandClient/QWaylandClientExtension>

class VirtualOutputManager
    : public QWaylandClientExtensionTemplate<VirtualOutputManager>
    , public QtWayland::treeland_virtual_output_manager_v1
{
    Q_OBJECT
public:
    explicit VirtualOutputManager();

    void virtual_output_manager_v1_virtual_output_list(wl_array *names) { }
};

VirtualOutputManager::VirtualOutputManager()
    : QWaylandClientExtensionTemplate<VirtualOutputManager>(1)
{
}

class VirtualOutput
    : public QWaylandClientExtensionTemplate<VirtualOutput>
    , public QtWayland::treeland_virtual_output_v1
{
    Q_OBJECT
public:
    explicit VirtualOutput(struct ::treeland_virtual_output_v1 *object);

    void virtual_output_v1_outputs(const QString &name, wl_array *outputs)
    {
        qInfo() << "Screen group name: " << name;
    }

    void virtual_output_v1_error(uint32_t code, const QString &message)
    {

        qInfo() << "error code:" << code << " error message:" << message;
    }
};

VirtualOutput::VirtualOutput(struct ::treeland_virtual_output_v1 *object)
    : QWaylandClientExtensionTemplate<VirtualOutput>(1)
    , QtWayland::treeland_virtual_output_v1(object)
{
}

// ./test-virtual-output HDMI-A-1 VGA-1

// 点击设置界面恢复按钮，恢复设置

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    VirtualOutputManager manager;

    QObject::connect(
        &manager,
        &VirtualOutputManager::activeChanged,
        &manager,
        [&manager, argc, argv] {
            if (manager.isActive()) {
                QWidget *widget = new QWidget;
                widget->setAttribute(Qt::WA_TranslucentBackground);
                widget->resize(640, 480);

                QPushButton *button = new QPushButton("Restore settings", widget);
                button->setGeometry(0, 0, 150, 50);

                widget->show();

                QWindow *window = widget->windowHandle();

                if (window && window->handle()) {
                    QtWaylandClient::QWaylandWindow *waylandWindow =
                        static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
                    QList<QtWaylandClient::QWaylandScreen *> screens =
                        waylandWindow->display()->screens();

                    if (argc < 3) {
                        qInfo() << "Please refer to the following input screen "
                                   "name: ";
                        qInfo() << "  ./test-virtual-output HDMI-A-1 VGA-1  ";
                        for (auto *screen : screens) {
                            QString address = screen->name();
                            qInfo() << "Screen name is: " << address;
                        }
                        exit(0);
                    }

                    QList<QString> screeNames;
                    for (int i = 1; i < argc; ++i) {
                        screeNames.append(QString::fromUtf8(argv[i]));
                    }

                    // 实际使用需要判断主屏（被镜像的屏幕），将主屏放在wl_array的第一个,复制屏幕依次填充
                    if (!screens.isEmpty()) {
                        QByteArray screenNameArray;
                        for (auto screen : screeNames) {
                            screenNameArray.append(screen.toUtf8());
                            screenNameArray.append('\0');
                        }

                        // screenNmaeArray.append("HDMI-test"); // test error
                        screenNameArray.append('\0');

                        VirtualOutput *screen_output = new VirtualOutput(
                            manager.create_virtual_output("copyscreen1",
                                                          screenNameArray)); //"copyscreen1":
                                                                             // 客户端自定义分组名称

                        QObject::connect(button, &QPushButton::clicked, [screen_output]() {
                            screen_output->destroy();
                        });
                    }
                }
            }
        });

    return app.exec();
}

#include "main.moc"
