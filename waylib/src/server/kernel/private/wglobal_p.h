// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wglobal.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class WScopedListenerList;

class WAYLIB_SERVER_EXPORT WObjectPrivate
{
public:
    static WObjectPrivate *get(WObject *qq);

    // Cross-object listener bookkeeping used by WObject::listeners(owner),
    // removeListeners(), and teardown(). Kept here so WObject's public
    // surface does not expose the internal graph edges.
    void registerListenerTarget(WObject *target);
    void unregisterListenerTarget(WObject *target);
    bool removeListenersInternal(WObject *owner);

    virtual ~WObjectPrivate();

    // Live listener graph; public so unit tests can assert ownership edges
    // without friending WObjectPrivate.
    // WScopedListenerList instances created by listeners()/listeners(owner);
    // released by teardown() or removeListeners(). ~WObject requires empty.
    QList<std::pair<WObject *, WScopedListenerList *>> attachedListenerLists;
    // Targets this WObject registered on via other->listeners(this).
    QList<WObject *> attachedListenerTargets;

protected:
    WObjectPrivate(WObject *qq);

    inline int indexOfAttachedData(const void *owner) const {
        for (int i = 0; i < attachedDatas.count(); ++i)
            if (attachedDatas.at(i).first == owner)
                return i;
        return -1;
    }

    WObject *q_ptr;
    QList<std::pair<const void*, void*>> attachedDatas;

    W_DECLARE_PUBLIC(WObject)

};

WAYLIB_SERVER_END_NAMESPACE
