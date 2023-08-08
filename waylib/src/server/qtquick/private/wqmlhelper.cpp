// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wqmlhelper_p.h"

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQmlHelper::WQmlHelper(QObject *parent)
    : QObject{parent}
{

}

void WQmlHelper::itemStackToTop(QQuickItem *item)
{
    auto parent = item->parentItem();
    if (!parent)
        return;
    auto children = parent->childItems();
    if (children.count() < 2)
        return;
    item->stackAfter(children.last());
}

WAYLIB_SERVER_END_NAMESPACE
