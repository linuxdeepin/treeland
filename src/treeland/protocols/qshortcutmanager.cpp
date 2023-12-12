// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qshortcutmanager.h"

#include "shortcut_manager_impl.h"

#include <qwsignalconnector.h>
#include <qwglobal.h>
#include <qwdisplay.h>

#include <QHash>

QW_USE_NAMESPACE

class QTreeLandShortcutManagerV1Private : public QWObjectPrivate
{
public:
    QTreeLandShortcutManagerV1Private(treeland_shortcut_manager_v1 *handle, bool isOwner, QTreeLandShortcutManagerV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &QTreeLandShortcutManagerV1Private::on_destroy);
        sc.connect(&handle->events.new_context, this, &QTreeLandShortcutManagerV1Private::on_new_context);
    }
    ~QTreeLandShortcutManagerV1Private() {
        if (!m_handle)
            return;
        destroy();
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_new_context(void *);

    static QHash<void*, QTreeLandShortcutManagerV1*> map;
    QW_DECLARE_PUBLIC(QTreeLandShortcutManagerV1)
    QWSignalConnector sc;
};
QHash<void*, QTreeLandShortcutManagerV1*> QTreeLandShortcutManagerV1Private::map;

void QTreeLandShortcutManagerV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void QTreeLandShortcutManagerV1Private::on_new_context(void *data)
{
    auto *d = QTreeLandShortcutContextV1::from(static_cast<treeland_shortcut_context_v1*>(data));
    Q_EMIT q_func()->newContext(d);
}

QTreeLandShortcutManagerV1::QTreeLandShortcutManagerV1(treeland_shortcut_manager_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QTreeLandShortcutManagerV1Private(handle, isOwner, this))
{

}

QTreeLandShortcutManagerV1 *QTreeLandShortcutManagerV1::get(treeland_shortcut_manager_v1 *handle)
{
    return QTreeLandShortcutManagerV1Private::map.value(handle);
}

QTreeLandShortcutManagerV1 *QTreeLandShortcutManagerV1::from(treeland_shortcut_manager_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QTreeLandShortcutManagerV1(handle, false);
}

QTreeLandShortcutManagerV1 *QTreeLandShortcutManagerV1::create(QWDisplay *display)
{
    auto *handle = treeland_shortcut_manager_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new QTreeLandShortcutManagerV1(handle, true);
}

class QTreeLandShortcutContextV1Private : public QWObjectPrivate
{
public:
    QTreeLandShortcutContextV1Private(treeland_shortcut_context_v1 *handle, bool isOwner, QTreeLandShortcutContextV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &QTreeLandShortcutContextV1Private::on_destroy);
    }
    ~QTreeLandShortcutContextV1Private() {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner) {
            treeland_shortcut_context_v1_destroy(q_func()->handle());
        }
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);

    static QHash<void*, QTreeLandShortcutContextV1*> map;
    QW_DECLARE_PUBLIC(QTreeLandShortcutContextV1)
    QWSignalConnector sc;
};
QHash<void*, QTreeLandShortcutContextV1*> QTreeLandShortcutContextV1Private::map;

void QTreeLandShortcutContextV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

QTreeLandShortcutContextV1::QTreeLandShortcutContextV1(treeland_shortcut_context_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QTreeLandShortcutContextV1Private(handle, isOwner, this))
{

}

QTreeLandShortcutContextV1 *QTreeLandShortcutContextV1::get(treeland_shortcut_context_v1 *handle)
{
    return QTreeLandShortcutContextV1Private::map.value(handle);
}

QTreeLandShortcutContextV1 *QTreeLandShortcutContextV1::from(treeland_shortcut_context_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QTreeLandShortcutContextV1(handle, false);
}

void QTreeLandShortcutContextV1::happend()
{
    treeland_shortcut_context_v1_send_shortcut(handle());
}

void QTreeLandShortcutContextV1::registerFailed()
{
    treeland_shortcut_context_v1_send_register_failed(handle());
}
