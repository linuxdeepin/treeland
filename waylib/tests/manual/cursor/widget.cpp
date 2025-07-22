// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "widget.h"

#include <QCursor>
#include <QGridLayout>
#include <QLabel>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Qt Cursor Test");
    auto *layout = new QGridLayout;
    auto *w1 = new QLabel("wait");
    w1->setCursor(Qt::WaitCursor);
    layout->addWidget(w1, 0, 0);

    auto *w2 = new QLabel("Busy");
    w2->setCursor(Qt::BusyCursor);
    layout->addWidget(w2, 0, 1);

    auto *w3 = new QLabel("Cross");
    w3->setCursor(Qt::CrossCursor);
    layout->addWidget(w3, 1, 0);

    auto *w4 = new QLabel("Forbidden");
    w4->setCursor(Qt::ForbiddenCursor);
    layout->addWidget(w4, 1, 1);

    auto *w5 = new QLabel("Blank");
    w5->setCursor(Qt::BlankCursor);
    layout->addWidget(w5, 2, 0);

    auto *w6 = new QLabel("Custom");
    QPixmap pixmap(":/res/HandCursor.png");
    QSize picSize(16, 16);
    QPixmap scaledPixmap = pixmap.scaled(picSize, Qt::KeepAspectRatio);
    QCursor customCursor = QCursor(scaledPixmap);
    w6->setCursor(customCursor);
    layout->addWidget(w6, 2, 1);

    setLayout(layout);
}

Widget::~Widget()
{
}

