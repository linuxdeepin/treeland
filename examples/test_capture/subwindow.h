// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQuickWindow>

class SubWindow : public QQuickWindow
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *parent READ parent WRITE setParent NOTIFY parentChanged FINAL)
Q_SIGNALS:
    void parentChanged();

public:
    SubWindow();
    QQuickWindow *parent() const;
    void setParent(QQuickWindow *w);
};
