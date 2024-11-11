// Copyright (C) 2024 justforlxz <zhangdingyuan@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dde_shell.h"

#include <private/qwaylandscreen_p.h>

#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <qpa/qplatformscreen.h>

class TestWindow : public QWidget
{
    Q_OBJECT
public:
    TestWindow()
        : m_manager(new DDEShell)
    {
        setWindowTitle("Window Overlap Checker");
        QVBoxLayout *mainLayout = new QVBoxLayout;
        QLabel *label = new QLabel("Not Overlapped");

        mainLayout->addWidget(label, 0, Qt::AlignCenter);
        setLayout(mainLayout);

        setMinimumSize(400, 300);

        connect(m_manager, &DDEShell::activeChanged, this, [this, label] {
            qDebug() << "ddeshell manager active changed";

            if (m_manager->isActive()) {
                m_checker =
                    new DDEShellWindowOverlapChecker(m_manager->get_window_overlap_checker());
                m_checker->update(
                    30,
                    30,
                    QtWayland::treeland_window_overlap_checker::anchor::anchor_bottom,
                    static_cast<QtWaylandClient::QWaylandScreen *>(screen()->handle())->output());
                connect(m_checker,
                        &DDEShellWindowOverlapChecker::overlappedChanged,
                        this,
                        [this, label] {
                            label->setText(m_checker->overlapped() ? "Overlapped"
                                                                   : "Not Overlapped");
                        });
            }
        });
    }

    ~TestWindow()
    {
        if (m_manager != nullptr) {
            delete m_manager;
            m_manager = nullptr;
        }
    }

private:
    DDEShell *m_manager{ nullptr };
    DDEShellWindowOverlapChecker *m_checker{ nullptr };
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    TestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
