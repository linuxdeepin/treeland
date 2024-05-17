// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "shortcut_manager_impl.h"

#include <qwglobal.h>

#include <QObject>

QW_USE_NAMESPACE

QW_BEGIN_NAMESPACE
class QWOutput;
class QWDisplay;
QW_END_NAMESPACE

class QTreeLandShortcutContextV1;
class QTreeLandShortcutManagerV1Private;

class QW_EXPORT QTreeLandShortcutManagerV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QTreeLandShortcutManagerV1)
public:
    inline treeland_shortcut_manager_v1 *handle() const
    {
        return QWObject::handle<treeland_shortcut_manager_v1>();
    }

    static QTreeLandShortcutManagerV1 *get(treeland_shortcut_manager_v1 *handle);
    static QTreeLandShortcutManagerV1 *from(treeland_shortcut_manager_v1 *handle);
    static QTreeLandShortcutManagerV1 *create(QWDisplay *display);

Q_SIGNALS:
    void beforeDestroy(QTreeLandShortcutManagerV1 *self);
    void newContext(QTreeLandShortcutContextV1 *context);

private:
    QTreeLandShortcutManagerV1(treeland_shortcut_manager_v1 *handle, bool isOwner);
    ~QTreeLandShortcutManagerV1() = default;
};

class QTreeLandShortcutContextV1Private;

class QTreeLandShortcutContextV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QTreeLandShortcutContextV1)
public:
    ~QTreeLandShortcutContextV1() = default;

    inline treeland_shortcut_context_v1 *handle() const
    {
        return QWObject::handle<treeland_shortcut_context_v1>();
    }

    static QTreeLandShortcutContextV1 *get(treeland_shortcut_context_v1 *handle);
    static QTreeLandShortcutContextV1 *from(treeland_shortcut_context_v1 *handle);

    void happend();
    void registerFailed();

Q_SIGNALS:
    void beforeDestroy(QTreeLandShortcutContextV1 *self);

private:
    QTreeLandShortcutContextV1(treeland_shortcut_context_v1 *handle, bool isOwner);
};
