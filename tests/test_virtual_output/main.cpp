// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-virtual-output-manager-v1.h"

#include <private/qwaylandwindow_p.h>
#include <qwindow.h>

#include <private/qwaylandscreen_p.h>
#include <QApplication>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

class VirtualOutputManager : public QWaylandClientExtensionTemplate<VirtualOutputManager>,
                              public QtWayland::virtual_output_manager_v1
{
    Q_OBJECT
public:
    explicit VirtualOutputManager();

    void treeland_virtual_output_manager_v1_virtual_output_list(wl_array *names)
    {
        // qInfo() << "-------screens name----- " << state;
    }
};

VirtualOutputManager::VirtualOutputManager()
    : QWaylandClientExtensionTemplate<VirtualOutputManager>(1)
{
}

class VirtualOutput : public QWaylandClientExtensionTemplate<VirtualOutput>,
                              public QtWayland::virtual_output_v1
{
    Q_OBJECT
public:
    explicit VirtualOutput(struct ::virtual_output_v1 *object);

    void treeland_virtual_output_v1_outputs(const QString &name, wl_array *outputs){
        qInfo() << "-------Screen group name---- " << name;
    }

    void treeland_virtual_output_v1_error(uint32_t code, const QString &message){

        qInfo() << "error code:"<<code << " error message:" << message;

    }
};

VirtualOutput::VirtualOutput(struct ::virtual_output_v1 *object)
    : QWaylandClientExtensionTemplate<VirtualOutput>(1)
    , QtWayland::virtual_output_v1(object)
{
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    VirtualOutputManager manager;

    QObject::connect(&manager, &VirtualOutputManager::activeChanged, &manager, [&manager] {
        if (manager.isActive()) {
            QWidget *widget = new QWidget;
            widget->setAttribute(Qt::WA_TranslucentBackground);
            widget->resize(640, 480);

            widget->show();

            QWindow *window = widget->windowHandle();

            if (window && window->handle()) {
                QtWaylandClient::QWaylandWindow *waylandWindow =
                    static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());

                qInfo() << "--------waylandWindow->waylandScreen()->name()---------" << waylandWindow->waylandScreen()->name();

                struct wl_output *output = waylandWindow->waylandScreen()->output();

                QList<QtWaylandClient::QWaylandScreen *> screens = waylandWindow->display()->screens();
                qInfo() << "--------screens---------" << screens.size();

                // 实际使用需要判断主屏（被镜像的屏幕），将主屏放在wl_array的第一个,复制屏幕依次填充
                if (!screens.isEmpty()){
                    QByteArray screenNmaeArray;
                    for(auto *screen : screens) {
                        QString address = screen->name();
                        screenNmaeArray.append(address.toUtf8());
                        screenNmaeArray.append('\0');
                    }
                    screenNmaeArray.append('\0');

                    VirtualOutput *screen_output =
                        new VirtualOutput(manager.create_virtual_output("copyscreen1",screenNmaeArray)); //"copyscreen1": 客户端自定义分组名称

                }
            }
        }
    });

    return app.exec();
}

#include "main.moc"
