# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# GLES2 / Vulkan shader generation and DRM pnpids for waylib-wlroots.
# Reuses upstream embed.sh and gen_pnpids.sh; Vulkan via glslang -V --vn.

function(wlroots_generate_shaders target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "wlroots_generate_shaders: target '${target}' does not exist")
    endif()
    if(NOT WLROOTS_SOURCE_DIR)
        message(FATAL_ERROR "wlroots_generate_shaders: WLROOTS_SOURCE_DIR is not set")
    endif()

    # ------------------------------------------------------------------
    # GLES2: embed GLSL into C string headers via render/gles2/shaders/embed.sh
    # ------------------------------------------------------------------
    if(WLR_HAS_GLES2_RENDERER)
        set(_gles2_shader_dir "${WLROOTS_SOURCE_DIR}/render/gles2/shaders")
        set(_gles2_embed_sh "${_gles2_shader_dir}/embed.sh")
        set(_gles2_out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/gles2")
        file(MAKE_DIRECTORY "${_gles2_out_dir}")

        set(_gles2_shaders
            common.vert
            quad.frag
            tex_rgba.frag
            tex_rgbx.frag
            tex_external.frag
        )

        set(_gles2_generated)
        foreach(_shader IN LISTS _gles2_shaders)
            set(_shader_path "${_gles2_shader_dir}/${_shader}")
            # meson underscorify: '.' → '_'
            string(REPLACE "." "_" _base "${_shader}")
            set(_var "${_base}_src")
            set(_output "${_gles2_out_dir}/${_base}_src.h")

            # sh -c for stdin/stdout redirection (VERBATIM does not invoke a shell)
            add_custom_command(
                OUTPUT "${_output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_gles2_out_dir}"
                COMMAND sh -c "\"$1\" \"$2\" < \"$3\" > \"$4\""
                        --
                        "${_gles2_embed_sh}"
                        "${_var}"
                        "${_shader_path}"
                        "${_output}"
                DEPENDS "${_shader_path}" "${_gles2_embed_sh}"
                COMMENT "embed GLES2 shader ${_shader}"
                VERBATIM
            )
            list(APPEND _gles2_generated "${_output}")
        endforeach()

        target_sources(${target} PRIVATE ${_gles2_generated})
        target_include_directories(${target} PRIVATE "${_gles2_out_dir}")
        set_source_files_properties(${_gles2_generated} PROPERTIES GENERATED TRUE)
    endif()

    # ------------------------------------------------------------------
    # Vulkan: glslang -V --vn <name>_data → render/vulkan/shaders/*.h
    # Includes use "render/vulkan/shaders/common.vert.h", so keep that layout
    # under CMAKE_CURRENT_BINARY_DIR (already a PRIVATE include).
    # ------------------------------------------------------------------
    if(WLR_HAS_VULKAN_RENDERER)
        if(NOT GLSLANG_VALIDATOR)
            message(FATAL_ERROR "wlroots_generate_shaders: GLSLANG_VALIDATOR is not set")
        endif()

        set(_vk_shader_dir "${WLROOTS_SOURCE_DIR}/render/vulkan/shaders")
        set(_vk_out_dir "${CMAKE_CURRENT_BINARY_DIR}/render/vulkan/shaders")
        file(MAKE_DIRECTORY "${_vk_out_dir}")

        set(_vk_shaders
            common.vert
            texture.frag
            quad.frag
            output.frag
        )

        # glslang 11+ supports --quiet (matches meson).
        execute_process(
            COMMAND "${GLSLANG_VALIDATOR}" --version
            OUTPUT_VARIABLE _glslang_ver_out
            ERROR_VARIABLE _glslang_ver_err
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _glslang_ver_rc
        )
        set(_glslang_ver_text "${_glslang_ver_out}\n${_glslang_ver_err}")
        set(_glslang_quiet_args)
        if(_glslang_ver_text MATCHES "Glslang Version: ([0-9]+):")
            if(CMAKE_MATCH_1 GREATER_EQUAL 11)
                set(_glslang_quiet_args --quiet)
            endif()
        elseif(_glslang_ver_text MATCHES "([0-9]+)\\.[0-9]+")
            if(CMAKE_MATCH_1 GREATER_EQUAL 11)
                set(_glslang_quiet_args --quiet)
            endif()
        endif()

        set(_vk_generated)
        foreach(_shader IN LISTS _vk_shaders)
            set(_shader_path "${_vk_shader_dir}/${_shader}")
            string(REPLACE "." "_" _base "${_shader}")
            set(_var "${_base}_data")
            set(_output "${_vk_out_dir}/${_shader}.h")

            add_custom_command(
                OUTPUT "${_output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_vk_out_dir}"
                COMMAND "${GLSLANG_VALIDATOR}"
                    -V "${_shader_path}"
                    -o "${_output}"
                    --vn "${_var}"
                    ${_glslang_quiet_args}
                DEPENDS "${_shader_path}"
                COMMENT "glslang Vulkan shader ${_shader}"
                VERBATIM
            )
            list(APPEND _vk_generated "${_output}")
        endforeach()

        target_sources(${target} PRIVATE ${_vk_generated})
        set_source_files_properties(${_vk_generated} PROPERTIES GENERATED TRUE)
    endif()
endfunction()

# DRM pnpids.c from hwdata pnp.ids via backend/drm/gen_pnpids.sh
function(wlroots_generate_pnpids target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "wlroots_generate_pnpids: target '${target}' does not exist")
    endif()
    if(NOT WLROOTS_SOURCE_DIR)
        message(FATAL_ERROR "wlroots_generate_pnpids: WLROOTS_SOURCE_DIR is not set")
    endif()

    if(NOT WLR_HAS_DRM_BACKEND)
        return()
    endif()

    if(NOT HWDATA_DIR)
        message(FATAL_ERROR "wlroots_generate_pnpids: HWDATA_DIR is not set")
    endif()

    set(_pnp_ids "${HWDATA_DIR}/pnp.ids")
    if(NOT EXISTS "${_pnp_ids}")
        message(FATAL_ERROR "wlroots_generate_pnpids: missing ${_pnp_ids}")
    endif()

    set(_gen_pnpids_sh "${WLROOTS_SOURCE_DIR}/backend/drm/gen_pnpids.sh")
    set(_pnpids_c "${CMAKE_CURRENT_BINARY_DIR}/pnpids.c")

    add_custom_command(
        OUTPUT "${_pnpids_c}"
        COMMAND sh -c "\"$1\" < \"$2\" > \"$3\""
                --
                "${_gen_pnpids_sh}"
                "${_pnp_ids}"
                "${_pnpids_c}"
        DEPENDS "${_pnp_ids}" "${_gen_pnpids_sh}"
        COMMENT "generate pnpids.c from hwdata"
        VERBATIM
    )

    target_sources(${target} PRIVATE "${_pnpids_c}")
    set_source_files_properties("${_pnpids_c}" PROPERTIES GENERATED TRUE)
endfunction()
