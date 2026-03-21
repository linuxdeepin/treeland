// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwobject.h"
#include <QtCore/private/qobject_p.h>

QW_BEGIN_NAMESPACE

QHash<void*, QObject*> qw_object_basic::map;

void qw_object_basic::cleanupDeclarativeData(QObject *obj)
{
    QObjectPrivate *d = QObjectPrivate::get(obj);
    if (d->declarativeData) {
        if (!d->isDeletingChildren && QAbstractDeclarativeData::destroyed)
            QAbstractDeclarativeData::destroyed(d->declarativeData, obj);
        d->declarativeData = nullptr;
    }
}

QW_END_NAMESPACE
