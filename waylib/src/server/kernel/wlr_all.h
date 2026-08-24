// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

// Feature flags for guarding wlroots headers that only exist (or only
// compile) when the corresponding feature is enabled in the wlroots build.
// wconfig.h is part of the waylib public API surface (generated from the
// wlroots CMake export, installed alongside these headers), so the macros
// are visible in every translation unit that includes this header, even
// when it is pulled in directly (without wglobal.h).
#include <wconfig.h>

// Unified C++ wrapper for the wlroots C API.
//
// wlroots 0.19 headers do not guard themselves with extern "C" (unlike most
// C libraries), so every C++ translation unit that uses wlroots must include
// the wlroots headers inside an extern "C" block. This header is that single
// point of entry: include <wlr_all.h> instead of individual <wlr/...> headers.
//
// It also works around C++ keywords used as struct field names in a few
// wlroots headers ("namespace" in wlr_layer_shell_v1.h, "class" in
// wlr/xwayland/xwayland.h, "delete" in wlr_input_method_v2.h). The macros are
// #undef'd right after each include: field access in waylib code uses the
// expanded names (scope/_class/_delete).

// Qt's legacy "slots"/"signals" macros (defined unless QT_NO_KEYWORDS /
// QT_NO_SIGNALS_SLOTS_KEYWORDS is set) collide with wlroots struct field
// names (e.g. wlr_swapchain_slot slots[]). Undef them around the includes
// and restore afterwards so TU-local Qt keyword usage keeps working. TUs
// built with QT_NO_SIGNALS_SLOTS_KEYWORDS (waylib itself) skip this.
#if defined(slots) && !defined(QT_NO_SIGNALS_SLOTS_KEYWORDS)
#define WLRINC_HAD_SLOTS_MACRO
#undef slots
#endif
#if defined(signals) && !defined(QT_NO_SIGNALS_SLOTS_KEYWORDS)
#define WLRINC_HAD_SIGNALS_MACRO
#undef signals
#endif

// C99 allows 'static' as an array-size hint in parameter declarations (e.g.
// float matrix[static 9]), but this is not valid C++ syntax.  Under
// extern "C" the C++ compiler still parses the declaration, so we must
// hide the keyword for the duration of the wlroots includes.
#define static
#ifdef __cplusplus
extern "C" {
#endif


#include <wlr/backend.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/headless.h>
#if defined(WLR_HAVE_LIBINPUT_BACKEND)
#include <wlr/backend/libinput.h>
#endif
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/egl.h>
#if defined(WLR_HAVE_GLES2_RENDERER)
#include <wlr/render/gles2.h>
#endif
#include <wlr/render/interface.h>
#include <wlr/render/pixman.h>
#include <wlr/render/swapchain.h>
#if defined(WLR_HAVE_VULKAN_RENDERER)
#include <wlr/render/vulkan.h>
#endif
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_toplevel_tag_v1.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/xcursor.h>

// "namespace" is a C++ keyword; wlr_layer_surface_v1 uses it as a field name.
#define namespace scope
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace

// "class" is a C++ keyword; wlr_xwayland_surface uses it as a field name.
// wlr/xwayland/xwayland.h pulls in xcb.h -> pthread.h, and glibc's pthread
// header declares C++ cleanup classes, so pthread.h must be parsed before
// the redefinition macro below (its include guard then keeps it from being
// re-expanded inside the macro block).
#if defined(WLR_HAVE_XWAYLAND)
#include <pthread.h>
// The class macro below must never see pthread.h's C++ cleanup classes;
// it is only safe because the include guard keeps the already-parsed header
// from being re-expanded. Enforce the ordering so a future include reorder
// fails loudly instead of silently corrupting C++ declarations.
#ifndef _PTHREAD_H
#error "wlr_all.h: <pthread.h> must be included before the 'class' redefinition block (xcb.h pulls it in transitively)"
#endif
#define class _class
#include <wlr/xwayland.h>
#include <wlr/xwayland/server.h>
#include <wlr/xwayland/shell.h>
#include <wlr/xwayland/xwayland.h>
#undef class
#endif

// "delete" is a C++ keyword; wlr_input_method_v2 uses it as a field name.
#define delete _delete
#include <wlr/types/wlr_input_method_v2.h>
#undef delete

// Vendored wlroots extension (implemented in buffer.c): live buffer count for
// shutdown leak assertions.
size_t waylib_buffer_get_count(void);

#ifdef __cplusplus
}
#endif
#undef static


#ifdef WLRINC_HAD_SLOTS_MACRO
#define slots Q_SLOTS
#undef WLRINC_HAD_SLOTS_MACRO
#endif
#ifdef WLRINC_HAD_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef WLRINC_HAD_SIGNALS_MACRO
#endif
