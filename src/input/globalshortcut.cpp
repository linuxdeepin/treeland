// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "globalshortcut.h"

#include <QHash>
#include <QKeySequence>

namespace Treeland {
class GlobalShortcutPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(GlobalShortcut)

    GlobalShortcut *q_ptr;
    QHash<const QAction*, QList<QKeySequence>> shortcuts;

public:
    explicit GlobalShortcutPrivate(GlobalShortcut *parent)
        : q_ptr(parent)
    {
    }

    void init()
    {
        // Initialization code for global shortcuts can go here.
    }
};

bool GlobalShortcut::setShortcut(QAction *action, const QList<QKeySequence> &shortcut)
{
    Q_D(GlobalShortcut);
    if (!action || shortcut.isEmpty()) {
        return false;
    }
    d->shortcuts[action] = shortcut;
    return true;
}

QList<QKeySequence> GlobalShortcut::shortcut(const QAction *action) const
{
    Q_D(const GlobalShortcut);
    return d->shortcuts.value(action);
}

bool GlobalShortcut::hasShortcut(const QAction *action) const
{
    Q_D(const GlobalShortcut);
    return d->shortcuts.contains(action);
}

void GlobalShortcut::removeAllShortcuts(QAction *action)
{
    Q_D(GlobalShortcut);
    d->shortcuts.remove(action);
}

GlobalShortcut::GlobalShortcut(QObject *parent)
    : QObject(parent)
    , d_ptr(new GlobalShortcutPrivate(this))
{
    Q_D(GlobalShortcut);
    d->init();
}
} // namespace Treeland

#include "globalshortcut.moc"
