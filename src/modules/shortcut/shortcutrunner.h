// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include "shortcutmanager.h"

class ShortcutRunner : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutRunner(QObject *parent = nullptr);

public Q_SLOTS:
    void onActionTrigger(ShortcutAction action, const QString &name, bool repeat);
    void onActionProgress(ShortcutAction action, qreal progress, const QString &name);
    void onActionFinish(ShortcutAction action, const QString &name);

private:
    void updateWorkspaceSwipe(qreal cb);
    void finishWorkspaceSwipe();

    qreal m_desktopOffset = 0;
    int m_fromId = 0;
    int m_toId = 0;
    bool m_slideEnable = false;
    bool m_slideBounce = false;
};
