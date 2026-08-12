// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wglobal.h>

#include <wayland-util.h>

#include <type_traits>
#include <utility>

#include <deque>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Extracts the first parameter type of a lambda/functor's operator().
template<typename T>
struct LambdaTraits : LambdaTraits<decltype(&std::remove_reference_t<T>::operator())> {};
template<typename C, typename R, typename A>
struct LambdaTraits<R (C::*)(A)> {
    using FirstArgument = A;
};
template<typename C, typename R, typename A>
struct LambdaTraits<R (C::*)(A) const> {
    using FirstArgument = A;
};
template<typename C, typename R, typename A>
struct LambdaTraits<R (C::*)(A) noexcept> {
    using FirstArgument = A;
};
template<typename C, typename R, typename A>
struct LambdaTraits<R (C::*)(A) const noexcept> {
    using FirstArgument = A;
};
template<typename C, typename R>
struct LambdaTraits<R (C::*)()> {
    using FirstArgument = void;
};
template<typename C, typename R>
struct LambdaTraits<R (C::*)() const> {
    using FirstArgument = void;
};
template<typename C, typename R>
struct LambdaTraits<R (C::*)() noexcept> {
    using FirstArgument = void;
};
template<typename C, typename R>
struct LambdaTraits<R (C::*)() const noexcept> {
    using FirstArgument = void;
};

// RAII wrapper for wl_signal: auto wl_signal_add on init, auto wl_list_remove
// on destruction. Supports member-function and lambda callbacks.
// Never forwards wl_signal to Qt signals.
//
// Deliberately standard-layout: the trampoline recovers the owning instance
// with W_CONTAINER_OF (offsetof), which requires a standard-layout type.
// Callbacks are type-erased through a heap-allocated closure + function
// pointers instead of std::function.
class WAYLIB_SERVER_EXPORT WScopedListener
{
public:
    WScopedListener() = default;
    ~WScopedListener() {
        disconnect();
        if (m_deleter)
            m_deleter(m_context);
    }

    WScopedListener(WScopedListener &&other) noexcept;
    WScopedListener &operator=(WScopedListener &&other) noexcept;

    WScopedListener(const WScopedListener &) = delete;
    WScopedListener &operator=(const WScopedListener &) = delete;

    // Member function callback: void (T::*)(Event*); T may be a base class of obj.
    template<typename Base, typename Derived, typename Event, typename R>
    void init(wl_signal *signal, Derived *obj, R (Base::*func)(Event *)) {
        initLambda(signal, [obj, func](void *data) {
            (obj->*func)(static_cast<Event *>(data));
        });
    }
    template<typename Base, typename Derived, typename Event, typename R>
    void init(wl_signal *signal, Derived *obj, R (Base::*func)(Event *) const) {
        initLambda(signal, [obj, func](void *data) {
            (obj->*func)(static_cast<Event *>(data));
        });
    }
    // Member function callback without arguments: void (T::*)()
    template<typename Base, typename Derived, typename R>
    void init(wl_signal *signal, Derived *obj, R (Base::*func)()) {
        initLambda(signal, [obj, func](void * /*data*/) {
            (obj->*func)();
        });
    }
    template<typename Base, typename Derived, typename R>
    void init(wl_signal *signal, Derived *obj, R (Base::*func)() const) {
        initLambda(signal, [obj, func](void * /*data*/) {
            (obj->*func)();
        });
    }

    // Lambda / functor callback with arbitrary event-pointer parameter.
    // The lambda's single parameter type is extracted via operator() and the
    // raw void* data is converted to it at invoke time.
    template<typename Callable>
    requires (!std::is_member_function_pointer_v<std::remove_reference_t<Callable>>)
    void init(wl_signal *signal, void * /*obj*/, Callable &&callback) {
        initLambda(signal, std::forward<Callable>(callback));
    }
    // 2-arg form for plain callbacks without an object.
    template<typename Callable>
    requires (!std::is_member_function_pointer_v<std::remove_reference_t<Callable>>)
    void init(wl_signal *signal, Callable &&callback) {
        initLambda(signal, std::forward<Callable>(callback));
    }

    void disconnect();
    [[nodiscard]] bool isConnected() const { return m_connected; }
    wl_listener *listener() { return &m_listener; }

private:
    template<typename Callable>
    void initLambda(wl_signal *signal, Callable &&callback) {
        // const-qualified lvalue lambdas must still be storable in a void*.
        using CallableType = std::remove_cv_t<std::remove_reference_t<Callable>>;
        static_assert(requires { typename LambdaTraits<CallableType>::FirstArgument; },
            "WScopedListener: callback must take zero or one argument "
            "(a pointer to the event type).");
        disconnect();
        // Release the previous closure (if any): disconnect() only unlinks
        // the wl_listener node — the heap-allocated closure survives until
        // here and must be freed before overwriting m_context/m_deleter,
        // otherwise re-init leaks the old closure.
        if (m_deleter) {
            m_deleter(m_context);
            m_callback = nullptr;
            m_deleter = nullptr;
            m_context = nullptr;
        }
        // Heap-allocate the closure: standard-layout forbids storing it
        // inline (std::function would break layout). The closure is kept in
        // a shared_ptr so that destroying this listener from inside its own
        // callback (a very common pattern for wl_signal destroy handlers)
        // only drops one reference — the callback keeps executing on its own
        // copy and the closure is freed when the emission returns.
        m_context = new std::shared_ptr<CallableType>(
            std::make_shared<CallableType>(std::forward<Callable>(callback)));
        m_callback = &invoke<CallableType>;
        m_deleter = &destroy<CallableType>;
        m_listener.notify = &WScopedListener::trampoline;
        m_signal = signal;
        wl_signal_add(signal, &m_listener);
        m_connected = true;
    }
    template<typename Callable>
    static void invoke(void *ctx, void *data) {
        using ArgType = typename LambdaTraits<Callable>::FirstArgument;
        // Take a local copy of the shared_ptr first: the owning listener may
        // be destroyed (and its closure released) while this callback runs.
        auto closure = *static_cast<std::shared_ptr<Callable> *>(ctx);
        if constexpr (std::is_same_v<ArgType, void>) {
            (*closure)();
        } else {
            (*closure)(static_cast<ArgType>(data));
        }
    }
    template<typename Callable>
    static void destroy(void *ctx) {
        delete static_cast<std::shared_ptr<Callable> *>(ctx);
    }

    static void trampoline(wl_listener *listener, void *data);

    // The signal this listener is attached to; needed to re-link the
    // wl_listener node on move (the node address changes, the signal does
    // not). Null while disconnected.
    wl_signal *m_signal = nullptr;
    wl_listener m_listener {};
    void (*m_callback)(void *, void *) = nullptr;
    void (*m_deleter)(void *) = nullptr;
    void *m_context = nullptr;
    bool m_connected = false;
};

// A growable set of wl_signal listeners owned together. Every listener is
// detached when the list is cleared or destroyed, so a wrapper can keep a
// list as a member and never manage individual lifetimes. Not copyable:
// moving re-links the wl_listener nodes (WScopedListener keeps m_signal).
class WAYLIB_SERVER_EXPORT WScopedListenerList
{
public:
    WScopedListenerList() = default;
    ~WScopedListenerList() = default;
    WScopedListenerList(WScopedListenerList &&) noexcept = default;
    WScopedListenerList &operator=(WScopedListenerList &&) noexcept = default;
    WScopedListenerList(const WScopedListenerList &) = delete;
    WScopedListenerList &operator=(const WScopedListenerList &) = delete;

    template<typename... Args>
    void add(Args&&... args) {
        // Init into a temporary first: if init throws (heap alloc), the
        // temporary cleans up and the deque is untouched. Move into the
        // deque only on success — deque push_back never relocates existing
        // elements, so their wl_listener node addresses stay stable.
        WScopedListener tmp;
        tmp.init(std::forward<Args>(args)...);
        m_listeners.push_back(std::move(tmp));
    }

    // Detach every registered listener. Safe to call from a callback that
    // belongs to this list; the list itself may even be destroyed right
    // after (closures are shared_ptr-managed and outlive the emission).
    void disconnect() {
        for (auto &listener : m_listeners)
            listener.disconnect();
    }

    // Detach and drop every registered listener. Safe from inside a
    // callback of this list: each closure keeps itself alive while its
    // callback is running.
    void clear() {
        disconnect();
        m_listeners.clear();
    }

private:
    // std::deque: WScopedListener is move-only and deque growth never
    // relocates existing elements — moving a WScopedListener would re-add
    // its wl_listener at the signal tail.
    std::deque<WScopedListener> m_listeners;
};

WAYLIB_SERVER_END_NAMESPACE
