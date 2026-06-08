// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwinterface.h>
#include <qwsignalconnector.h>

#include <QHash>
#include <QMetaMethod>

QW_BEGIN_NAMESPACE

class QW_EXPORT qw_object_basic : public QObject {
    Q_OBJECT

public:
    qw_object_basic (QObject *parent): QObject(parent) { }

    template<typename T>
    static inline QList<T*> get_objects() {
        QList<T*> list;

        for (const auto &i : std::as_const(map)) {
            if (auto obj = qobject_cast<T*>(i))
                list << obj;
        }

        return list;
    }

Q_SIGNALS:
    void before_destroy();

protected:
    static QHash<void*, QObject*> map;
};

template<typename Handle, typename Derive>
class qw_object : public qw_object_basic
{
public:
    typedef Handle HandleType;
    typedef Derive DeriveType;

    qw_object(Handle *h, bool isOwner, QObject *parent=nullptr)
        : qw_object_basic(parent)
        , m_handle(h)
        , isHandleOwner(isOwner)
    {
        static_assert(QtPrivate::HasQ_OBJECT_Macro<Derive>::Value,
                      "Please add Q_OBJECT macro to the derive class.");

        Q_ASSERT(!map.contains(h));
        map.insert((void*)(h), this);

        constexpr bool has_destroy_signal = requires(const Handle& h) {
            h.events.destroy;
        };

        if constexpr (has_destroy_signal) {
            sc.connect(&h->events.destroy, this, &qw_object::on_destroy);
        }
    }

    ~qw_object() {
        if (!m_handle)
            return;
        do_destroy();

        if (isHandleOwner) {
            constexpr bool has_destroy = requires(Derive t) {
                t.destroy();
            };

            if constexpr (has_destroy) {
                static_cast<Derive*>(this)->Derive::destroy();
            } else {
                qFatal("qwl_wrap_object(%p) can't to destroy, maybe its ownership is wl_display.",
                       static_cast<void*>(this));
            }
        }
    }

    static QW_ALWAYS_INLINE QList<Derive*> get_objects() {
        return qw_object_basic::get_objects<Derive>();
    }

    static QW_ALWAYS_INLINE Derive *get(Handle *handle) {
        return qobject_cast<Derive*>(map.value(handle));
    }

    static QW_ALWAYS_INLINE Derive *from(Handle *handle) {
        if (!handle)
            return nullptr;

        if (auto o = get(handle))
            return o;

        constexpr bool has_create_from_handle = requires(Derive *d, Handle *handle) {
            d = Derive::create(handle);
        };
        if constexpr (has_create_from_handle) {
            return Derive::create(handle);
        } else {
            return new Derive(handle, false);
        }
    }

    template<class Interface, typename... Args>
    static QW_ALWAYS_INLINE DeriveType *create(Args&&... args)
        requires (std::is_base_of_v<qw_interface_basic, Interface>
                 && std::is_same_v<HandleType, std::remove_reference_t<decltype(*(std::declval<Interface>().handle()))>>) {
        Interface *i = new Interface();
        return create(i, std::forward<Args>(args)...);
    }

    template<class Interface, typename... Args>
    static QW_ALWAYS_INLINE DeriveType *create(Interface *i, Args&&... args)
        requires (std::is_base_of_v<qw_interface_basic, Interface>
                 && std::is_same_v<HandleType, std::remove_reference_t<decltype(*i->handle())>>) {
        i->init(std::forward<Args>(args)...);
        return new DeriveType(i->handle(), true);
    }

    QW_ALWAYS_INLINE bool is_valid() const {
        // NOTE(lxz): Some functions of wlroots allow null pointer parameters. In order to reduce repeated verification code fragments, If this ptr is nullptr, return nullptr.
        // WARNING(lxz): Check this in the member function, it is UB. Under some compilers it is necessary to use `volatile` to prevent compiler optimizations.
        //               In the derived class, if the object address is nullptr, the address of this is not necessarily 0x0, it may be 0x01. The correct memory address starting position will not be lower than 0x1000, so it is considered that addresses lower than 0x1000 are nullptr.
        volatile auto thisPtr = reinterpret_cast<qintptr>(this);
        return thisPtr > 0x1000 && m_handle;
    }

    QW_ALWAYS_INLINE Handle *handle() const {
        if (!is_valid())
            return nullptr;
        return m_handle;
    }

    QW_ALWAYS_INLINE Handle *operator -> () const {
        return handle();
    }

    QW_ALWAYS_INLINE operator Handle* () const {
        return handle();
    }

    QW_ALWAYS_INLINE void set_data(void* owner, void* data) {
        if (m_data.first && owner) {
            Q_ASSERT(m_data.first == owner);
        }

        m_data = std::make_pair(owner, data);
    }

    template<typename Data>
    QW_ALWAYS_INLINE Data* get_data() const {
        return reinterpret_cast<Data*>(m_data.second);
    }

    template <typename S, typename SS>
    void bind_signal(S s, SS qt_signal) {
        typedef typename QtPrivate::FunctionPointer<SS>::Object DeriveType;
        static_assert(std::is_base_of<qw_object, DeriveType>::value,
                      "The signal is not defined in the derive class of qw_object");
        auto obj = static_cast<DeriveType*>(this);
        sc.connect(&(obj->handle()->events.*s), obj, qt_signal);
    }

protected:
    inline void do_destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains((void*)m_handle));

        sc.invalidate();
        map.remove((void*)m_handle);
    }
    inline void on_destroy() {
        Q_EMIT before_destroy();

        do_destroy();
        m_handle = nullptr;
        delete this;
    }

    Handle *m_handle;
    bool isHandleOwner;
    qw_signal_connector sc;

private:
    Q_DISABLE_COPY(qw_object)
    std::pair<void*, void*> m_data; // <owner, data>
};

#define QW_CLASS_OBJECT(wlr_type_suffix) \
QW_EXPORT qw_##wlr_type_suffix : public qw_object<wlr_##wlr_type_suffix, qw_##wlr_type_suffix>

#define QW_OBJECT \
protected: \
using qw_object::qw_object; \
friend class qw_object;

#define QW_SIGNAL(name, ...) \
Q_SIGNALS: \
    void notify_##name(__VA_ARGS__); \
private: \
struct qw_signal_##name { \
    qw_signal_##name(DeriveType *self) { \
        self->bind_signal(&decltype(self->handle()->events)::name, &DeriveType::notify_##name); \
    } \
}; \
qw_signal_##name _signal_##name = this; \

QW_END_NAMESPACE
