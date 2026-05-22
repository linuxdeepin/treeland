// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-xdg-toplevel-tag-v1.h"

#include <private/qwaylandwindow_p.h>

#include <any>
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtWaylandClient/QWaylandClientExtension>

class XdgToplevelTagManager
    : public QWaylandClientExtensionTemplate<XdgToplevelTagManager>
    , public QtWayland::xdg_toplevel_tag_manager_v1
{
    Q_OBJECT
public:
    explicit XdgToplevelTagManager()
        : QWaylandClientExtensionTemplate<XdgToplevelTagManager>(1)
    {
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    XdgToplevelTagManager manager;

    QMainWindow window;
    window.setWindowTitle("Toplevel Tag Test");
    window.resize(400, 300);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);

    auto *tagEdit = new QLineEdit("main-window", central);
    tagEdit->setPlaceholderText("Enter tag...");
    auto *descEdit = new QLineEdit("Main Window", central);
    descEdit->setPlaceholderText("Enter description...");
    auto *setTagBtn = new QPushButton("Set Tag", central);
    auto *setDescBtn = new QPushButton("Set Description", central);
    auto *statusLabel = new QLabel(central);

    layout->addWidget(tagEdit);
    layout->addWidget(descEdit);
    layout->addWidget(setTagBtn);
    layout->addWidget(setDescBtn);
    layout->addWidget(statusLabel);
    window.setCentralWidget(central);

    window.show();

    QTimer::singleShot(500, &window, [&] {
        if (!manager.isActive()) {
            statusLabel->setText("Error: compositor does not support xdg_toplevel_tag_manager_v1");
            qWarning() << "xdg_toplevel_tag_manager_v1 not available";
            return;
        }

        QWindow *handle = window.windowHandle();
        if (!handle || !handle->handle()) {
            qWarning() << "window handle not ready";
            return;
        }

        auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(handle->handle());
        auto *shell = waylandWindow->shellSurface();
        if (!shell) {
            qWarning() << "no shell surface";
            return;
        }

        auto role = shell->surfaceRole();
        auto *rawValue = std::any_cast<struct ::xdg_toplevel *>(&role);
        if (!rawValue || !*rawValue) {
            qWarning() << "failed to get xdg_toplevel from surfaceRole";
            return;
        }
        auto *raw = *rawValue;
        qDebug() << "xdg_toplevel ready:" << (void *)raw;

        QObject::connect(setTagBtn,
                         &QPushButton::clicked,
                         &window,
                         [raw, &manager, tagEdit, statusLabel] {
                             const QString tag = tagEdit->text();
                             if (!tag.isEmpty()) {
                                 manager.set_toplevel_tag(raw, tag);
                                 statusLabel->setText(QString("Tag set: %1").arg(tag));
                             }
                         });

        QObject::connect(setDescBtn,
                         &QPushButton::clicked,
                         &window,
                         [raw, &manager, descEdit, statusLabel] {
                             const QString desc = descEdit->text();
                             if (!desc.isEmpty()) {
                                 manager.set_toplevel_description(raw, desc);
                                 statusLabel->setText(QString("Description set: %1").arg(desc));
                             }
                         });

        statusLabel->setText("Ready");
    });

    return app.exec();
}

#include "main.moc"
