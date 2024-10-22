// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QWidget>

class DDEShelSurfaceWindow : public QWidget
{
    Q_OBJECT
public:
    enum TestMode
    {
        TestSetPosition,
        TestSetAutoPlace
    };

    explicit DDEShelSurfaceWindow(TestMode mode, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void apply();

private:
    TestMode m_mode;
};
