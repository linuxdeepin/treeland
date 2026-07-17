# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Wayland protocol codegen for waylib-wlroots.
# Mirrors protocol/meson.build: private-code + server-header for all protocols,
# client-header for wayland backend client_protos.

function(wlroots_generate_protocols target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "wlroots_generate_protocols: target '${target}' does not exist")
    endif()

    if(NOT WAYLAND_SCANNER_BIN)
        message(FATAL_ERROR "wlroots_generate_protocols: WAYLAND_SCANNER_BIN is not set")
    endif()
    if(NOT WAYLAND_PROTOCOLS_DATADIR)
        message(FATAL_ERROR "wlroots_generate_protocols: WAYLAND_PROTOCOLS_DATADIR is not set")
    endif()

    set(_wlr_proto_dir "${CMAKE_CURRENT_BINARY_DIR}/protocol")
    file(MAKE_DIRECTORY "${_wlr_proto_dir}")

    if(NOT WLROOTS_SOURCE_DIR)
        message(FATAL_ERROR "wlroots_generate_protocols: WLROOTS_SOURCE_DIR is not set")
    endif()
    set(_wlr_local_proto_dir "${WLROOTS_SOURCE_DIR}/protocol")
    set(_wlr_wp_dir "${WAYLAND_PROTOCOLS_DATADIR}")

    # name → xml path (absolute). Matches protocol/meson.build keys and paths.
    set(_wlr_protocol_pairs
        # Stable upstream protocols
        "linux-dmabuf-v1|${_wlr_wp_dir}/stable/linux-dmabuf/linux-dmabuf-v1.xml"
        "presentation-time|${_wlr_wp_dir}/stable/presentation-time/presentation-time.xml"
        "tablet-v2|${_wlr_wp_dir}/stable/tablet/tablet-v2.xml"
        "viewporter|${_wlr_wp_dir}/stable/viewporter/viewporter.xml"
        "xdg-shell|${_wlr_wp_dir}/stable/xdg-shell/xdg-shell.xml"

        # Staging upstream protocols
        "alpha-modifier-v1|${_wlr_wp_dir}/staging/alpha-modifier/alpha-modifier-v1.xml"
        "color-management-v1|${_wlr_wp_dir}/staging/color-management/color-management-v1.xml"
        "content-type-v1|${_wlr_wp_dir}/staging/content-type/content-type-v1.xml"
        "cursor-shape-v1|${_wlr_wp_dir}/staging/cursor-shape/cursor-shape-v1.xml"
        "drm-lease-v1|${_wlr_wp_dir}/staging/drm-lease/drm-lease-v1.xml"
        "ext-foreign-toplevel-list-v1|${_wlr_wp_dir}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
        "ext-idle-notify-v1|${_wlr_wp_dir}/staging/ext-idle-notify/ext-idle-notify-v1.xml"
        "ext-image-capture-source-v1|${_wlr_wp_dir}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml"
        "ext-image-copy-capture-v1|${_wlr_wp_dir}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml"
        "ext-session-lock-v1|${_wlr_wp_dir}/staging/ext-session-lock/ext-session-lock-v1.xml"
        "ext-data-control-v1|${_wlr_wp_dir}/staging/ext-data-control/ext-data-control-v1.xml"
        "fractional-scale-v1|${_wlr_wp_dir}/staging/fractional-scale/fractional-scale-v1.xml"
        "linux-drm-syncobj-v1|${_wlr_wp_dir}/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml"
        "security-context-v1|${_wlr_wp_dir}/staging/security-context/security-context-v1.xml"
        "single-pixel-buffer-v1|${_wlr_wp_dir}/staging/single-pixel-buffer/single-pixel-buffer-v1.xml"
        "xdg-activation-v1|${_wlr_wp_dir}/staging/xdg-activation/xdg-activation-v1.xml"
        "xdg-dialog-v1|${_wlr_wp_dir}/staging/xdg-dialog/xdg-dialog-v1.xml"
        "xdg-system-bell-v1|${_wlr_wp_dir}/staging/xdg-system-bell/xdg-system-bell-v1.xml"
        "xdg-toplevel-icon-v1|${_wlr_wp_dir}/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml"
        "xwayland-shell-v1|${_wlr_wp_dir}/staging/xwayland-shell/xwayland-shell-v1.xml"
        "tearing-control-v1|${_wlr_wp_dir}/staging/tearing-control/tearing-control-v1.xml"
        "ext-transient-seat-v1|${_wlr_wp_dir}/staging/ext-transient-seat/ext-transient-seat-v1.xml"

        # Unstable upstream protocols
        "idle-inhibit-unstable-v1|${_wlr_wp_dir}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml"
        "keyboard-shortcuts-inhibit-unstable-v1|${_wlr_wp_dir}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml"
        "pointer-constraints-unstable-v1|${_wlr_wp_dir}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
        "pointer-gestures-unstable-v1|${_wlr_wp_dir}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml"
        "primary-selection-unstable-v1|${_wlr_wp_dir}/unstable/primary-selection/primary-selection-unstable-v1.xml"
        "relative-pointer-unstable-v1|${_wlr_wp_dir}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
        "text-input-unstable-v3|${_wlr_wp_dir}/unstable/text-input/text-input-unstable-v3.xml"
        "xdg-decoration-unstable-v1|${_wlr_wp_dir}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
        "xdg-foreign-unstable-v1|${_wlr_wp_dir}/unstable/xdg-foreign/xdg-foreign-unstable-v1.xml"
        "xdg-foreign-unstable-v2|${_wlr_wp_dir}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml"
        "xdg-output-unstable-v1|${_wlr_wp_dir}/unstable/xdg-output/xdg-output-unstable-v1.xml"

        # Local protocols under protocol/
        "drm|${_wlr_local_proto_dir}/drm.xml"
        "input-method-unstable-v2|${_wlr_local_proto_dir}/input-method-unstable-v2.xml"
        "kde-server-decoration|${_wlr_local_proto_dir}/server-decoration.xml"
        "virtual-keyboard-unstable-v1|${_wlr_local_proto_dir}/virtual-keyboard-unstable-v1.xml"
        "wlr-data-control-unstable-v1|${_wlr_local_proto_dir}/wlr-data-control-unstable-v1.xml"
        "wlr-export-dmabuf-unstable-v1|${_wlr_local_proto_dir}/wlr-export-dmabuf-unstable-v1.xml"
        "wlr-foreign-toplevel-management-unstable-v1|${_wlr_local_proto_dir}/wlr-foreign-toplevel-management-unstable-v1.xml"
        "wlr-gamma-control-unstable-v1|${_wlr_local_proto_dir}/wlr-gamma-control-unstable-v1.xml"
        "wlr-layer-shell-unstable-v1|${_wlr_local_proto_dir}/wlr-layer-shell-unstable-v1.xml"
        "wlr-output-management-unstable-v1|${_wlr_local_proto_dir}/wlr-output-management-unstable-v1.xml"
        "wlr-output-power-management-unstable-v1|${_wlr_local_proto_dir}/wlr-output-power-management-unstable-v1.xml"
        "wlr-screencopy-unstable-v1|${_wlr_local_proto_dir}/wlr-screencopy-unstable-v1.xml"
        "wlr-virtual-pointer-unstable-v1|${_wlr_local_proto_dir}/wlr-virtual-pointer-unstable-v1.xml"
    )

    # Client headers required by backend/wayland (backend/wayland/meson.build).
    set(_wlr_client_protos
        drm
        linux-dmabuf-v1
        linux-drm-syncobj-v1
        pointer-gestures-unstable-v1
        presentation-time
        relative-pointer-unstable-v1
        tablet-v2
        viewporter
        xdg-activation-v1
        xdg-decoration-unstable-v1
        xdg-shell
    )

    set(_wlr_proto_sources)
    set(_wlr_proto_headers)

    foreach(_pair IN LISTS _wlr_protocol_pairs)
        string(REPLACE "|" ";" _parts "${_pair}")
        list(GET _parts 0 _name)
        list(GET _parts 1 _xml)

        if(NOT EXISTS "${_xml}")
            message(FATAL_ERROR "wlroots_generate_protocols: missing protocol XML for '${_name}': ${_xml}")
        endif()

        # wayland-scanner @BASENAME@ is the XML filename without directory/extension.
        get_filename_component(_basename "${_xml}" NAME_WE)

        set(_out_c "${_wlr_proto_dir}/${_basename}-protocol.c")
        set(_out_server_h "${_wlr_proto_dir}/${_basename}-protocol.h")

        add_custom_command(
            OUTPUT "${_out_c}" "${_out_server_h}"
            COMMAND "${WAYLAND_SCANNER_BIN}" private-code "${_xml}" "${_out_c}"
            COMMAND "${WAYLAND_SCANNER_BIN}" server-header "${_xml}" "${_out_server_h}"
            DEPENDS "${_xml}"
            COMMENT "wayland-scanner private-code/server-header ${_basename}"
            VERBATIM
        )

        list(APPEND _wlr_proto_sources "${_out_c}")
        list(APPEND _wlr_proto_headers "${_out_server_h}")

        if(_name IN_LIST _wlr_client_protos)
            set(_out_client_h "${_wlr_proto_dir}/${_basename}-client-protocol.h")
            add_custom_command(
                OUTPUT "${_out_client_h}"
                COMMAND "${WAYLAND_SCANNER_BIN}" client-header "${_xml}" "${_out_client_h}"
                DEPENDS "${_xml}"
                COMMENT "wayland-scanner client-header ${_basename}"
                VERBATIM
            )
            list(APPEND _wlr_proto_headers "${_out_client_h}")
        endif()
    endforeach()

    target_sources(${target} PRIVATE ${_wlr_proto_sources} ${_wlr_proto_headers})
    # BEFORE: parent projects (qwlroots) may inject older system wlr-protocols
    # headers via global include_directories(); those must not win.
    target_include_directories(${target} BEFORE PRIVATE "${_wlr_proto_dir}")

    set_source_files_properties(${_wlr_proto_sources} ${_wlr_proto_headers}
        PROPERTIES GENERATED TRUE
    )
endfunction()
