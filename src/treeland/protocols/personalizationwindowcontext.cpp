// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only


#include "personalizationmanager.h"
#include "personalization_manager_impl.h"

#include <QHash>

#include <qwoutput.h>
#include <qwsignalconnector.h>
#include <qwglobal.h>

QW_USE_NAMESPACE

class PersonalizationWindowContextPrivate : public QWObjectPrivate
{
public:
    PersonalizationWindowContextPrivate(personalization_window_context_v1 *handle, bool isOwner, PersonalizationWindowContext *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &PersonalizationWindowContextPrivate::on_destroy);
        sc.connect(&handle->events.set_background_type, this, &PersonalizationWindowContextPrivate::on_set_background_type);
    }
    ~PersonalizationWindowContextPrivate() {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            personalization_window_context_v1_destroy(q_func()->handle());
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_set_background_type(void *);

    static QHash<void*, PersonalizationWindowContext*> map;
    QW_DECLARE_PUBLIC(PersonalizationWindowContext)
    QWSignalConnector sc;
};
QHash<void*, PersonalizationWindowContext*> PersonalizationWindowContextPrivate::map;

void PersonalizationWindowContextPrivate::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void PersonalizationWindowContextPrivate::on_set_background_type(void *data)
{
    if (auto *p = reinterpret_cast<personalization_window_context_v1*>(data)) {
        Q_EMIT q_func()->backgroundTypeChanged(PersonalizationWindowContext::from(p));
    }
}

PersonalizationWindowContext::PersonalizationWindowContext(personalization_window_context_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new PersonalizationWindowContextPrivate(handle, isOwner, this))
{

}

PersonalizationWindowContext *PersonalizationWindowContext::get(personalization_window_context_v1 *handle)
{
    return PersonalizationWindowContextPrivate::map.value(handle);
}

PersonalizationWindowContext *PersonalizationWindowContext::from(personalization_window_context_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new PersonalizationWindowContext(handle, false);
}

PersonalizationWindowContext *PersonalizationWindowContext::create(personalization_window_context_v1 *handle)
{
    if (!handle)
        return nullptr;
    return new PersonalizationWindowContext(handle, true);
}
