// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

QW_BEGIN_NAMESPACE

class qw_interface_basic {};

template<typename Handle, typename Impl>
class qw_interface : public qw_interface_basic
{
protected:
    struct _handle : public Handle {
        qw_interface *interface;
    };

public:
    typedef Handle HandleType;
    typedef Impl ImplType;

    QW_ALWAYS_INLINE Handle *handle() const {
        return m_handle;
    }

    QW_ALWAYS_INLINE Impl *impl() const {
        return m_handleImpl;
    }

    QW_ALWAYS_INLINE operator Impl* () const {
        return impl();
    }

    QW_ALWAYS_INLINE operator Handle* () const {
        return handle();
    }

    QW_ALWAYS_INLINE static qw_interface *get(Handle *handle) {
        auto h = static_cast<_handle*>(handle);
        Q_ASSERT(h->interface && h->impl == handle->impl);
        return h->interface;
    }

protected:
    template <typename I, I i, typename II, II ii>
    struct qw_interface_binder {
        template<typename T>
        struct caller {};

        template <class Obj, typename Ret, typename ...Args>
        struct caller<Ret (Obj::*) (Args...)> {
            typedef Ret return_type;

            static Ret call(HandleType *handle, Args ... args) {
                auto interface = static_cast<Obj*>(static_cast<_handle*>(handle)->interface);
                return (interface->*ii)(std::forward<Args>(args)...);
            }
        };

        typedef caller<II> callback;

        static inline void bind(qw_interface *self) {
            self->m_handleImpl->*i = callback::call;
        }

        qw_interface_binder(qw_interface *self) {
            bind(self);
        }
    };

    static void destroy(Handle *handle) {
        auto self = get(handle);
        Q_ASSERT(self);
        delete self;
    }

    qw_interface()
        : m_handle(reinterpret_cast<_handle*>(calloc(1, sizeof(_handle))))
        , m_handleImpl(new Impl {})
    {
        static_cast<_handle*>(m_handle)->interface = this;
        constexpr bool has_destroy = requires(const Impl &i) {
            i.destroy;
        };

        if constexpr (has_destroy) {
            m_handleImpl->destroy = destroy;
        }
    }

    virtual ~qw_interface() {
        free(handle());
        delete impl();
    }

    Handle *m_handle = nullptr;
    Impl *m_handleImpl = nullptr;
};

#define QW_INTERFACE_INIT(wlr_type_suffix) \
friend class qw_interface; \
friend class qw_##wlr_type_suffix; \
private: \
using qw_interface::qw_interface; \
QW_MAYBE_FUNC_STATIC(wlr_type_suffix, finish) \
public: \
~qw_##wlr_type_suffix##_interface() { finish(*this); } \
template<typename ...Args> \
QW_ALWAYS_INLINE void init(Args &&... args) { \
    wlr_##wlr_type_suffix##_##init(*this, *this, std::forward<Args>(args)...); \
}

#define QW_CLASS_INTERFACE(wlr_type_suffix) \
class qw_##wlr_type_suffix##_interface : public qw_interface<wlr_##wlr_type_suffix, wlr_##wlr_type_suffix##_impl> { \
    QW_INTERFACE_INIT(wlr_type_suffix) \
};

#define QW_INTERFACE(name, ret_type, ...) \
protected: \
template <typename I> \
struct qw_interface_##name { \
    template <typename II> \
    qw_interface_##name(II *self) { \
        constexpr static bool has_interface = requires(const I &i) { \
            i.name; \
        }; \
        static_assert(has_interface, "Not found \"" QT_STRINGIFY(name) "\", Please check your wlroot's version."); \
        qw_interface_binder<decltype(&I::name), &I::name, decltype(&II::name), &II::name>::bind(self); \
    } \
}; \
qw_interface_##name<ImplType> _interface_##name = this; \
ret_type name(__VA_ARGS__)

#define QW_INTERFACE_FUNC_STATIC(wlr_type_suffix, wlr_func_suffix, ret_type, ...) \
template<typename ...Args> \
    QW_ALWAYS_INLINE static ret_type \
    wlr_func_suffix(Args &&... args) requires std::is_invocable_v<void(*)(__VA_ARGS__), Args...> \
{ \
        static_assert(std::is_invocable_v<decltype(wlr_##wlr_type_suffix##_##wlr_func_suffix), Args...>, ""); \
        return wlr_##wlr_type_suffix##_##wlr_func_suffix(std::forward<Args>(args)...); \
}

QW_END_NAMESPACE
