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
        QPushButton *button1 = new QPushButton("set window light theme");
        QPushButton *button2 = new QPushButton("set window dark theme");
        group1Layout->addWidget(button1);
        group1Layout->addWidget(button2);
        mainLayout->addLayout(group1Layout);
        connect(button1, &QPushButton::clicked, this, [this, button1] {
            context->set_window_theme_type(
                TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT);
        });
        connect(button2, &QPushButton::clicked, this, [this, button1] {
            context->set_window_theme_type(
                TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK);
        });

        QLabel *currentWindowTheme = new QLabel;
        mainLayout->addWidget(currentWindowTheme);

        QLabel *titlebarLabel = new QLabel("titlebar height:");
        QLineEdit *titlebarEdit = new QLineEdit;
        QHBoxLayout *group2Layout = new QHBoxLayout;
        QPushButton *button3 = new QPushButton("set titlebar height");
        group2Layout->addWidget(titlebarLabel);
        group2Layout->addWidget(titlebarEdit);
        group2Layout->addWidget(button3);
        mainLayout->addLayout(group2Layout);

        connect(button3, &QPushButton::clicked, this, [this, titlebarEdit] {
            context->set_window_titlebar_height(titlebarEdit->text().toUInt());
        });

        mainLayout->addStretch();

        connect(m_manager,
                &PersonalizationManager::activeChanged,
                this,
                [this, currentWindowTheme, titlebarEdit] {
                    qDebug() << "personalization manager active changed";

                    if (m_manager->isActive()) {
                        context = new Appearance(m_manager->get_appearance_context());
                        cursor = new Cursor(m_manager->get_cursor_context());
                        font = new Font(m_manager->get_font_context());

                        connect(context,
                                &Appearance::windowThemeTypeChanged,
                                this,
                                [currentWindowTheme](uint32_t type) {
                                    currentWindowTheme->setText(
                                        QString("current window theme type: %1").arg(type));
                                });
                        context->get_window_theme_type();

                        connect(context,
                                &Appearance::windowTitlebarHeightChanged,
                                this,
                                [titlebarEdit](uint32_t height) {
                                    titlebarEdit->blockSignals(true);
                                    titlebarEdit->setText(QString::number(height));
                                    titlebarEdit->blockSignals(false);
                                    qDebug() << "titlebar height changed: " << height;
                                });

                        context->get_window_titlebar_height();
                        font->get_font();
                        font->get_font_size();
                        font->get_monospace_font();
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
    Font *font = nullptr;
    Cursor *cursor = nullptr;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    TestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
