// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WINDOW_H
#define WINDOW_H

#include <QQuickWindow>

class CustomWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(QWindow* parent READ parent WRITE setParent NOTIFY parentChanged FINAL)
    QML_NAMED_ELEMENT(CustomWindow)

public:
    explicit CustomWindow(QWindow *parent = nullptr)
        : QQuickWindow(parent) {}

    QWindow *parent() const;
    void setParent(QWindow *newParent);

signals:
    void parentChanged();

private:
    QWindow *m_parent = nullptr;
};

#endif // WINDOW_H
