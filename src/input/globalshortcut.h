// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

class QAction;

namespace Treeland {

class GlobalShortcutPrivate;

class GlobalShortcut : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(GlobalShortcut)
public:
    explicit GlobalShortcut(QObject *parent = nullptr);

    bool setShortcut(QAction *action, const QList<QKeySequence> &shortcut);
    QList<QKeySequence> shortcut(const QAction *action) const;
    bool hasShortcut(const QAction *action) const;
    void removeAllShortcuts(QAction *action);

private:
    GlobalShortcutPrivate *d_ptr;
};
} // namespace Treeland