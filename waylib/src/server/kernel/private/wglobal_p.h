// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wglobal.h"
#include "utils/wscopedlistener.h"
#include <qwobject.h>
#include <QHash>
#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WObjectPrivate
{
public:
    static WObjectPrivate *get(WObject *qq);

    virtual ~WObjectPrivate();
    virtual wl_client *waylandClient() const {
        return nullptr;
    }

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

class WAYLIB_SERVER_EXPORT WWrapObjectPrivate : public WObjectPrivate
{
public:
    WWrapObjectPrivate(WWrapObject *q);
    ~WWrapObjectPrivate();

    template<typename Handle>
    inline Handle *handle() const {
        return qobject_cast<Handle*>(m_handle.get());
    }

    template<typename Wlr>
    inline Wlr *nativeHandle() const {
        return static_cast<Wlr*>(m_nativeHandle);
    }

    static WWrapObject *fromNativeHandle(const void *handle);

protected:
    W_DECLARE_PUBLIC(WWrapObject)

    void initHandle(QW_NAMESPACE::qw_object_basic *handle);
    void initNativeHandle(void *handle, wl_signal *destroySignal);
    void invalidate();
    virtual void instantRelease() {}

    QList<QMetaObject::Connection> connectionsWithHandle;
    QPointer<QW_NAMESPACE::qw_object_basic> m_handle;
    void *m_nativeHandle = nullptr;
    WScopedListener m_destroyListener;
    uint invalidated:1;

private:
    static QHash<void*, WWrapObject*> &nativeHandleMap();
    void onNativeDestroy();
};

#define WWRAP_HANDLE_FUNCTIONS(QW, WLR) \
inline QW *handle() const { \
    return WWrapObjectPrivate::handle<QW>(); \
} \
\
inline WLR *nativeHandle() const { \
    return handle()->handle(); \
}

#define WWRAP_NATIVE_HANDLE_FUNCTIONS(WLR) \
inline WLR *nativeHandle() const { \
    return WWrapObjectPrivate::nativeHandle<WLR>(); \
} \
inline WLR *handle() const { \
    return nativeHandle(); \
}

WAYLIB_SERVER_END_NAMESPACE
