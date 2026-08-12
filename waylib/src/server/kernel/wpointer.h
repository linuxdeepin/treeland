// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Generic RAII smart pointers for wlroots C objects.
//
// wlroots objects are C structs allocated by wlroots; they MUST be released
// through their dedicated destroy functions (wlr_texture_destroy,
// wlr_output_destroy, ...) and never with plain `delete`. These wrappers make
// that automatic and additionally behave like QPointer for objects that carry
// a `events.destroy` wl_signal: the pointer is nulled as soon as the native
// object is destroyed by anyone, so you never hold a dangling handle.
//
//   WUniquePointer<wlr_texture> tex(wlr_texture_from_buffer(...));
//   // destroyed via wlr_texture_destroy() on scope exit
//
//   WPointer<wlr_output> out(someOutput);
//   // never dangles: nulled when someOutput is destroyed
//
// Types without a destroy signal (e.g. wlr_texture, wlr_renderer) still get
// RAII destruction; only the auto-null feature is unavailable for them.
// New types: add a WlrObjectTraits specialization (see the macro below).

#pragma once

#include "wglobal.h"
#include "wlr_all.h"
#include "wcontainerof.h"

#include <wayland-util.h>

#include <memory>
#include <type_traits>
#include <utility>

WAYLIB_SERVER_BEGIN_NAMESPACE

// RAII deleter for wlr_buffer references (replaces qwlroots' qw_buffer::unlocker).
struct WBufferUnlocker {
    static inline void cleanup(wlr_buffer *buffer) { if (buffer) wlr_buffer_unlock(buffer); }
    void operator()(wlr_buffer *buffer) const { cleanup(buffer); }
};

// RAII deleter that drops a wlr_buffer reference (replaces qwlroots' qw_buffer::droper).
struct WBufferDroper {
    static inline void cleanup(wlr_buffer *buffer) { if (buffer) wlr_buffer_drop(buffer); }
    void operator()(wlr_buffer *buffer) const { cleanup(buffer); }
};

// Convenience aliases for wlr_buffer smart pointers.
using WBufferUnlockPtr = std::unique_ptr<wlr_buffer, WBufferUnlocker>;
using WBufferDropPtr = std::unique_ptr<wlr_buffer, WBufferDroper>;

// Detects whether a wlr type carries a `events.destroy` wl_signal member.
template <typename T, typename = void>
struct WlrHasDestroySignal : std::false_type {};
template <typename T>
struct WlrHasDestroySignal<T,
    std::void_t<decltype(std::declval<T &>().events.destroy)>> : std::true_type {};

// Maps a wlr type to its destroy function. Specialize per type, either with
// the W_DECLARE_WLR_TRAITS macro below or with a custom struct exposing a
// static `destroy(T *)`.
template <typename T>
struct WlrObjectTraits;

template <typename T>
concept WlrDestroyable = requires(T *p) { WlrObjectTraits<T>::destroy(p); };

#define W_DECLARE_WLR_TRAITS(Class, DestroyFn) \
    template <> struct WlrObjectTraits<Class> { \
        static void destroy(Class *handle) { DestroyFn(handle); } \
    };

// Core specializations. Add more types as they get owned by smart pointers.
W_DECLARE_WLR_TRAITS(wlr_texture, wlr_texture_destroy)
W_DECLARE_WLR_TRAITS(wlr_output, wlr_output_destroy)
W_DECLARE_WLR_TRAITS(wlr_renderer, wlr_renderer_destroy)
W_DECLARE_WLR_TRAITS(wlr_allocator, wlr_allocator_destroy)
W_DECLARE_WLR_TRAITS(wlr_swapchain, wlr_swapchain_destroy)
W_DECLARE_WLR_TRAITS(wlr_cursor, wlr_cursor_destroy)
W_DECLARE_WLR_TRAITS(wlr_seat, wlr_seat_destroy)
W_DECLARE_WLR_TRAITS(wlr_backend, wlr_backend_destroy)
W_DECLARE_WLR_TRAITS(wlr_output_layout, wlr_output_layout_destroy)
W_DECLARE_WLR_TRAITS(wlr_ext_foreign_toplevel_handle_v1,
                     wlr_ext_foreign_toplevel_handle_v1_destroy)
W_DECLARE_WLR_TRAITS(wlr_foreign_toplevel_handle_v1,
                     wlr_foreign_toplevel_handle_v1_destroy)
W_DECLARE_WLR_TRAITS(wlr_xdg_popup, wlr_xdg_popup_destroy)
W_DECLARE_WLR_TRAITS(wlr_layer_surface_v1, wlr_layer_surface_v1_destroy)
W_DECLARE_WLR_TRAITS(wlr_xcursor_manager, wlr_xcursor_manager_destroy)
#undef W_DECLARE_WLR_TRAITS

// Observing pointer (QPointer semantics). Does not own the object; the handle
// is automatically nulled when the native object is destroyed (for types with
// a destroy signal). Copyable: every instance listens independently.
template <typename T>
class WPointer
{
    static_assert(WlrHasDestroySignal<T>::value,
        "WPointer<T>: T must carry an events.destroy wl_signal. "
        "Use a raw pointer or WUniquePointer (which supports types "
        "without a destroy signal via RAII traits) instead.");
public:
    WPointer() = default;
    explicit WPointer(T *handle) { assign(handle); }
    ~WPointer() { clear(); }

    WPointer(const WPointer &other) { assign(other.m_handle); }
    WPointer &operator=(const WPointer &other) {
        if (this != &other)
            assign(other.m_handle);
        return *this;
    }
    WPointer(WPointer &&other) noexcept {
        T *h = other.m_handle;
        other.clear();   // detach other's listener, null its handle
        assign(h);       // this watches h with its own fresh listener
    }
    WPointer &operator=(WPointer &&other) noexcept {
        if (this != &other) {
            T *h = other.m_handle;
            other.clear();
            assign(h);
        }
        return *this;
    }

    WPointer &operator=(T *handle) {
        assign(handle);
        return *this;
    }

    void assign(T *handle) {
        if (m_handle == handle)
            return;
        clear();
        m_handle = handle;
        if (handle) {
            wl_signal_add(&handle->events.destroy, &m_destroyListener);
            m_listening = true;
        }
    }
    void clear() {
        if (m_listening) {
            if (m_destroyListener.link.next != nullptr)
                wl_list_remove(&m_destroyListener.link);
            m_listening = false;
        }
        m_handle = nullptr;
    }

    T *get() const { return m_handle; }
    T *operator->() const { return m_handle; }
    T &operator*() const { return *m_handle; }
    // Implicit conversion like QPointer: lets observers be passed to
    // wlroots C APIs that take a raw T* without extra .get() calls.
    operator T *() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }
    bool isNull() const { return m_handle == nullptr; }

private:
    static void onNativeDestroy(wl_listener *listener, void *) {
        using Self = WPointer<T>;
        Self *self = container_of<Self>(listener, offsetof(Self, m_destroyListener));
        // wlr_signal_emit_safe() detaches the node before notifying (link is
        // zeroed); wl_signal_emit_mutable() leaves it attached — remove only
        // when it is still linked.
        if (self->m_destroyListener.link.next != nullptr)
            wl_list_remove(&self->m_destroyListener.link);
        self->m_listening = false;
        self->m_handle = nullptr;
    }

    T *m_handle = nullptr;
    bool m_listening = false;
    // notify is set once at construction; assign()/clear() only manage the
    // link and the m_listening flag, reducing the risk of a path forgetting
    // to set it.
    wl_listener m_destroyListener { {}, &WPointer<T>::onNativeDestroy };
};

// Deleter that releases a wlroots object through WlrObjectTraits<T>::destroy.
template <typename T, typename Traits = WlrObjectTraits<T>>
struct WUniqueDeleter
{
    // Check the Traits actually in use (not WlrObjectTraits<T>), so a
    // custom Traits passed as the second template argument is validated.
    static_assert(requires(T *p) { Traits::destroy(p); },
        "WUniqueDeleter<T>: Traits must provide destroy(T*). "
        "Add one with W_DECLARE_WLR_TRAITS(Class, DestroyFn) or pass a custom traits.");

    void operator()(T *handle) const {
        if (handle)
            Traits::destroy(handle);
    }
};

// Owning pointer: std::unique_ptr with the automatic wlroots deleter.
// Pure RAII ownership — it does not observe the native destroy signal. If
// you need to track whether the native object is still alive, hold a
// WPointer alongside.
template <typename T, typename Traits = WlrObjectTraits<T>>
using WUniquePointer = std::unique_ptr<T, WUniqueDeleter<T, Traits>>;

WAYLIB_SERVER_END_NAMESPACE
