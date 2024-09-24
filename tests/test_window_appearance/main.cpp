// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>

class TestWindow : public QWidget
{
    Q_OBJECT
public:
    TestWindow()
        : m_manager(new PersonalizationManager)
    {
        setWindowTitle("Setting Appearance Client");

        QVBoxLayout *mainLayout = new QVBoxLayout;

        QHBoxLayout *group1Layout = new QHBoxLayout;
        QPushButton *button1 = new QPushButton("set cursor theme");
        QLineEdit *lineEdit1 = new QLineEdit;
        lineEdit1->setPlaceholderText("input theme name");
        group1Layout->addWidget(button1);
        group1Layout->addWidget(lineEdit1);
        mainLayout->addLayout(group1Layout);
        connect(button1, &QPushButton::clicked, this, [this, lineEdit1] {
            QString theme = lineEdit1->text();
            qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << theme;
            context->set_cursor_theme(theme);
        });

        QHBoxLayout *group2Layout = new QHBoxLayout;
        QLabel *label = new QLabel("current cursor");
        QLabel *label2 = new QLabel;
        group2Layout->addWidget(label);
        group2Layout->addWidget(label2);
        mainLayout->addLayout(group2Layout);

        QHBoxLayout *group3Layout = new QHBoxLayout;
        QPushButton *button3 = new QPushButton("get cursor theme");
        group3Layout->addWidget(button3);
        mainLayout->addLayout(group3Layout);
        QObject::connect(button3, &QPushButton::clicked, this, [this] {
            context->get_cursor_theme();
        });

        connect(m_manager, &PersonalizationManager::activeChanged, this, [this, label2] {
            qDebug() << "personalization manager active changed";

            if (m_manager->isActive()) {
                context = new Appearance(m_manager->get_appearance_context());
                connect(context, &Appearance::cursorThemeChanged, label2, &QLabel::setText);
            }
        });
        setLayout(mainLayout);

        setMinimumSize(400, 300);
    }

    ~TestWindow()
    {
        if (m_manager != nullptr) {
            delete m_manager;
            m_manager = nullptr;
        }
    }

private:
    PersonalizationManager *m_manager = nullptr;
    Appearance *context = nullptr;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    TestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
