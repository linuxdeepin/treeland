// Copyright (C) 2024 rewine <luhongxu@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QApplication>
#include <QMainWindow>
#include <QMouseEvent>
#include <QDebug>
#include <QBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

#include "personalization_manager.h"

class TestWindow : public QWidget
{
    Q_OBJECT
public:
    TestWindow()
        : m_manager(new PersonalizationManager)
    {
        setWindowTitle("Setting crusor Client");

        connect(m_manager, &PersonalizationManager::activeChanged, this, [this] {
            qDebug() << "personalization manager active changed";

            if (m_manager->isActive()) {
                cursor_context = new PersonalizationCursor(m_manager->get_cursor_context());
            }
        });

        QVBoxLayout *mainLayout = new QVBoxLayout;

        QHBoxLayout *group1Layout = new QHBoxLayout;
        QPushButton *button1 = new QPushButton("set cursor theme");
        QLineEdit *lineEdit1 = new QLineEdit;
        lineEdit1->setPlaceholderText("input theme name");
        group1Layout->addWidget(button1);
        group1Layout->addWidget(lineEdit1);
        mainLayout->addLayout(group1Layout);
        QObject::connect(button1, &QPushButton::clicked, this, [this, lineEdit1] {
            QString theme = lineEdit1->text();
            qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << theme;
            cursor_context->set_theme(theme);
            cursor_context->commit();
        });

        QHBoxLayout *group2Layout = new QHBoxLayout;
        QPushButton *button2 = new QPushButton("set cursor size");
        QLineEdit *lineEdit2 = new QLineEdit;
        lineEdit2->setPlaceholderText("input cursor size");
        group2Layout->addWidget(button2);
        group2Layout->addWidget(lineEdit2);
        mainLayout->addLayout(group2Layout);
        QObject::connect(button2, &QPushButton::clicked, this, [this, lineEdit2] {
            int cursor_size = lineEdit2->text().toInt();
            qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << cursor_size;
            cursor_context->set_size(cursor_size);
            cursor_context->commit();
        });

        QHBoxLayout *group3Layout = new QHBoxLayout;
        QPushButton *button3 = new QPushButton("get cursor theme");
        QPushButton *button4 = new QPushButton("get cursor size(see log)");
        group3Layout->addWidget(button3);
        group3Layout->addWidget(button4);
        mainLayout->addLayout(group3Layout);
        QObject::connect(button3, &QPushButton::clicked, this, [this] {
            cursor_context->get_theme();
        });
        QObject::connect(button4, &QPushButton::clicked, this, [this] {
            cursor_context->get_size();
        });

        setLayout(mainLayout);

        setMinimumSize(400, 300);
    }

    ~TestWindow()
    {
        if (m_manager != nullptr)
        {
            delete m_manager;
            m_manager = nullptr;
        }
    }

private:
    PersonalizationManager *m_manager = nullptr;
    PersonalizationCursor* cursor_context = nullptr;
};

int main (int argc, char **argv)
{
    QApplication app(argc, argv);

    TestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
