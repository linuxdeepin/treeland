// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "wscoplistener.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <private/qobject_p_p.h>
#include <QCursor>
#include <QLoggingCategory>
#include <utility>

WAYLIB_SERVER_BEGIN_NAMESPACE

WObject::WObject()
    : w_d_ptr(new WObjectPrivate(this))
{
}

WObject::WObject(WObjectPrivate &dd, WObject *)
    : w_d_ptr(&dd)
{
}

int WObject::indexOfAttachedData(const void *owner) const
{
    W_DC(WObject);
    return d->indexOfAttachedData(owner);
}

const QList<std::pair<const void *, void *>> &WObject::attachedDatas() const
{
    W_DC(WObject);
    return d->attachedDatas;
}

QList<std::pair<const void *, void *>> &WObject::attachedDatas()
{
    W_D(WObject);
    return d->attachedDatas;
}

void WObject::teardown()
{
    W_D(WObject);
    const auto targets = d->attachedListenerTargets;
    d->attachedListenerTargets.clear();

    for (WObject *target : targets) {
        if (target)
            WObjectPrivate::get(target)->removeListenersInternal(this);
    }

    for (auto it = d->attachedListenerLists.begin(); it != d->attachedListenerLists.end(); ) {
        if (it->first != this)
            WObjectPrivate::get(it->first)->unregisterListenerTarget(this);
        delete it->second;
        it = d->attachedListenerLists.erase(it);
    }
}

WObject::~WObject()
{
    W_D(WObject);
    // Force derived classes / WListenerOwner to call teardown() before the
    // base destructor runs. Leaving listeners attached here is a use-after-
    // free risk once native wlroots objects are gone.
    if (!d->attachedListenerLists.isEmpty() || !d->attachedListenerTargets.isEmpty()) {
        qFatal("WObject %p destroyed with pending listeners "
               "(lists=%d, cross-object targets=%d). "
               "Call teardown() in the most-derived destructor "
               "(or before tearing down native handles).",
               static_cast<void *>(this),
               int(d->attachedListenerLists.size()),
               int(d->attachedListenerTargets.size()));
    }
}

WScopedListenerList *WObject::listeners()
{
    W_D(WObject);
    for (const auto &entry : std::as_const(d->attachedListenerLists)) {
        if (entry.first == this)
            return entry.second;
    }
    auto *list = new WScopedListenerList;
    d->attachedListenerLists.append({this, list});
    return list;
}

WScopedListenerList *WObject::listeners(WObject *owner)
{
    Q_ASSERT(owner);
    // Self-owned listeners must go through listeners(). Passing `this` here
    // is almost always a mistake that conflates the two APIs.
    Q_ASSERT_X(owner != this, "WObject::listeners",
               "Use listeners() for this object's own listener list; "
               "listeners(owner) is only for cross-object registration");
    W_D(WObject);
    WObjectPrivate::get(owner)->registerListenerTarget(this);
    for (const auto &entry : std::as_const(d->attachedListenerLists)) {
        if (entry.first == owner)
            return entry.second;
    }
    auto *list = new WScopedListenerList;
    d->attachedListenerLists.append({owner, list});
    return list;
}

void WObject::removeListeners(WObject *owner)
{
    Q_ASSERT(owner);
    W_D(WObject);
    if (d->removeListenersInternal(owner)) {
        WObjectPrivate::get(owner)->unregisterListenerTarget(this);
        return;
    }

    QStringList registeredOwners;
    registeredOwners.reserve(d->attachedListenerLists.size());
    for (const auto &entry : std::as_const(d->attachedListenerLists))
        registeredOwners.append(QString::asprintf("%p", entry.first));

    qCInfo(lcWlObject) << "removeListeners: no listener group for owner"
                       << QString::asprintf("%p", owner)
                       << "on object" << QString::asprintf("%p", this)
                       << "- registered owners:" << registeredOwners;
}

WObjectPrivate *WObjectPrivate::get(WObject *qq)
{
    return qq->d_func();
}

WObjectPrivate::WObjectPrivate(WObject *qq)
    : q_ptr(qq)
{

}

WObjectPrivate::~WObjectPrivate()
{

}

void WObjectPrivate::registerListenerTarget(WObject *target)
{
    if (!target || target == q_ptr)
        return;
    for (WObject *existing : std::as_const(attachedListenerTargets)) {
        if (existing == target)
            return;
    }
    attachedListenerTargets.append(target);
}

void WObjectPrivate::unregisterListenerTarget(WObject *target)
{
    attachedListenerTargets.removeAll(target);
}

bool WObjectPrivate::removeListenersInternal(WObject *owner)
{
    Q_ASSERT(owner);
    for (auto it = attachedListenerLists.begin(); it != attachedListenerLists.end(); ++it) {
        if (it->first == owner) {
            delete it->second;
            attachedListenerLists.erase(it);
            return true;
        }
    }
    return false;
}

bool WGlobal::isInvalidCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::Invalid;
}

bool WGlobal::isClientResourceCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::ClientResource;
}

WAYLIB_SERVER_END_NAMESPACE
