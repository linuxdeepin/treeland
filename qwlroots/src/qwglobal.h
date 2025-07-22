// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwconfig.h>
#include <QtCore/qglobal.h>
#include <qobjectdefs.h>

#include <concepts>
#include <type_traits>

#ifdef QW_NAMESPACE
#define QW_BEGIN_NAMESPACE namespace QW_NAMESPACE {
#define QW_END_NAMESPACE }
#define QW_USE_NAMESPACE using namespace QW_NAMESPACE;
#else
#define QW_NAMESPACE
#define QW_BEGIN_NAMESPACE
#define QW_END_NAMESPACE
#define QW_USE_NAMESPACE
#endif

#define QW_DECLARE_PRIVATE(Class) Q_DECLARE_PRIVATE_D(qGetPtrHelper(qw_d_ptr), Class)
#define QW_DECLARE_PUBLIC(Class) Q_DECLARE_PUBLIC(Class)
#define QW_PRIVATE_SLOT(Func) Q_PRIVATE_SLOT(d_func(), Func)

#if defined(QWLROOTS_LIBRARY)
#  define QW_EXPORT Q_DECL_EXPORT
#else
#  define QW_EXPORT Q_DECL_IMPORT
#endif

/*
 *  We disallow some class's destructor, they are binding of a wlroots struct
 *  which will be free in c language
 *  We use destroying delete to avoid call destructor after delete, but clang++
 *  still check destructor must not deleted nor private, we call abort() to ensure
 *  destructor never called in runtime
 *
 *  More info https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0722r3.html
 */
#ifdef __clang__
    #define QW_DISALLOW_DESTRUCTOR(Class) ~Class() { std::abort(); };
#else
    #define QW_DISALLOW_DESTRUCTOR(Class) ~Class() = delete;
#endif


/*
 * static_assert(false) is not allowed before CWG2518/P2593R1
 * https://wg21.link/P2593R1
 */
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ <= 12
#define QW_NO_STRICT_STATIC_ASSERT_FALSE(message) Q_ASSERT_X(false, "QW_NO_STRICT_STATIC_ASSERT", message);
#else
#define QW_NO_STRICT_STATIC_ASSERT_FALSE(message) static_assert(false, message);
#endif

#ifdef QT_NO_DEBUG
#define QW_ALWAYS_INLINE Q_ALWAYS_INLINE
#else
#define QW_ALWAYS_INLINE
#endif

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE
#endif

QW_BEGIN_NAMESPACE

namespace qw {
    template<typename Func>
    typename QtPrivate::FunctionPointer<Func>::ReturnType cfunc_return_type(Func);

#define QW_CFUNC_RETURN_TYPE(func) decltype(qw::cfunc_return_type(func))

    template<typename T>
    concept Destroyable = requires (T *x) { x->destroy(); };
}

class qw_basic { };

template<typename Handle, typename Derive>
class qw_reinterpret_cast : public qw_basic
{
public:
    typedef Handle HandleType;
    typedef Derive DeriveType;

    qw_reinterpret_cast() = delete;
    QW_DISALLOW_DESTRUCTOR(qw_reinterpret_cast)

    QW_ALWAYS_INLINE Handle *handle() const {
        return reinterpret_cast<Handle*>(const_cast<Derive*>(static_cast<const Derive*>(this)));
    }

    QW_ALWAYS_INLINE static Derive *from(Handle *handle) {
        return reinterpret_cast<Derive*>(handle);
    }

    QW_ALWAYS_INLINE operator Handle* () const {
        return handle();
    }

    QW_ALWAYS_INLINE void operator delete(qw_reinterpret_cast *p, std::destroying_delete_t) {
        if constexpr (qw::Destroyable<Derive>) {
            static_cast<Derive*>(p)->Derive::destroy();
        } else {
            QW_NO_STRICT_STATIC_ASSERT_FALSE("Can't destroy.")
        }
    }
};

template<typename Handle, typename Derive>
class qw_class_box
{
public:
    typedef Handle HandleType;
    typedef Derive DeriveType;

    qw_class_box() {
        if constexpr (requires { static_cast<DeriveType*>(this)->init(); }) {
            static_cast<DeriveType*>(this)->init();
        } else {
            Q_ASSERT_X(false, "qw_class_box", "No default constructor.");
        }
    }

    ~qw_class_box() {
        static_cast<DeriveType*>(this)->finish();
    }

    QW_ALWAYS_INLINE Handle *handle() const {
        return &m_handle;
    }

    QW_ALWAYS_INLINE operator Handle* () const {
        return handle();
    }

private:
    mutable Handle m_handle;
};

#define QW_CLASS_REINTERPRET_CAST(wlr_type_suffix) \
qw_##wlr_type_suffix : public qw_reinterpret_cast<wlr_##wlr_type_suffix, qw_##wlr_type_suffix>

#define QW_CLASS_BOX(wlr_type_suffix) \
qw_##wlr_type_suffix : public qw_class_box<wlr_##wlr_type_suffix, qw_##wlr_type_suffix>

#define QW_FUNC_MEMBER(wlr_type_suffix, wlr_func_suffix, ret_type, ...) \
template<typename ...Args> \
QW_ALWAYS_INLINE ret_type \
wlr_func_suffix(Args &&... args) const requires std::is_invocable_v<void(*)(__VA_ARGS__), Args...> \
{ \
    static_assert(std::is_invocable_v<decltype(wlr_##wlr_type_suffix##_##wlr_func_suffix), decltype(*this), Args...>, "Arguments is not invocable"); \
    return wlr_##wlr_type_suffix##_##wlr_func_suffix(*this, std::forward<Args>(args)...); \
}

#define QW_FUNC_STATIC(wlr_type_suffix, wlr_func_suffix, ret_type, ...) \
template<typename ...Args> \
QW_ALWAYS_INLINE static ret_type \
wlr_func_suffix(Args &&... args) requires std::is_invocable_v<void(*)(__VA_ARGS__), Args...> \
{ \
    static_assert(std::is_invocable_v<decltype(wlr_##wlr_type_suffix##_##wlr_func_suffix), Args...>, ""); \
    if constexpr (std::is_same_v<QW_CFUNC_RETURN_TYPE(wlr_##wlr_type_suffix##_##wlr_func_suffix), HandleType*>) { \
        static_assert(std::is_same_v<ret_type, DeriveType*>, ""); \
        auto *wlr_handle = wlr_##wlr_type_suffix##_##wlr_func_suffix(std::forward<Args>(args)...); \
        if constexpr (std::string_view(#wlr_func_suffix).find("create") != std::string_view::npos) { \
            if constexpr (std::is_base_of_v<qw_basic, DeriveType>) { \
                return DeriveType::from(wlr_handle); \
            } else { \
                return wlr_handle ? new DeriveType(wlr_handle, true) : nullptr; \
            } \
        } \
        else if constexpr (std::string_view(#wlr_func_suffix).find("from") != std::string_view::npos || std::string_view(#wlr_func_suffix).find("get") != std::string_view::npos) { \
            return DeriveType::from(wlr_handle); \
        } else { \
            QW_NO_STRICT_STATIC_ASSERT_FALSE("return wlroots native type, but is not 'create' nor 'from' func!") \
        } \
    } else { \
        return wlr_##wlr_type_suffix##_##wlr_func_suffix(std::forward<Args>(args)...); \
    } \
}

#define QW_MAYBE_FUNC_STATIC(wlr_type_suffix, wlr_func_suffix) \
template<typename ...Args> \
QW_ALWAYS_INLINE static void wlr_func_suffix(Args &&... args) { \
    constexpr bool exists = requires() { \
        wlr_##wlr_type_suffix##_##wlr_func_suffix(std::forward<Args>(args)...); \
    }; \
    if constexpr (exists) \
        wlr_##wlr_type_suffix##_##wlr_func_suffix(std::forward<Args>(args)...); \
}

QW_END_NAMESPACE
