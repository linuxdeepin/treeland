// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QSettings>
#include <QProcess>

class Shortcut {
public:
    Shortcut(const QString &path);

    virtual ~Shortcut() = default;

    void exec();

    QString shortcut();

private:
    QSettings m_settings;
};
