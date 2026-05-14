include(CMakeParseArguments)

function(impl_treeland)
    set(one_value_args NAME)
    set(multi_value_args SOURCE INCLUDE LINK)

    cmake_parse_arguments(
        PARSE_ARG_PREFIX
        ""
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT PARSE_ARG_PREFIX_NAME)
        message(FATAL_ERROR "NAME is a required argument!")
    endif()

    add_library(${PARSE_ARG_PREFIX_NAME} INTERFACE)

    target_sources(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_SOURCE}
    )

    target_include_directories(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_INCLUDE}
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    )

    target_link_libraries(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_LINK}

            # TODO: remove this
            Dtk6::Core
            Dtk6::Declarative
            Dtk6::SystemSettings
            Waylib::WaylibServer
            Qt6::Quick
            Qt6::QuickControls2
            Qt6::QuickPrivate
            Qt6::DBus
            Qt6::Concurrent
            PkgConfig::PIXMAN
            PkgConfig::WAYLAND
            PkgConfig::LIBINPUT
            # TODO: end remove
    )

    target_link_libraries(libtreeland
        PRIVATE
            ${PARSE_ARG_PREFIX_NAME}
    )
endfunction()

# Workaround for qt_add_shaders bug: paths containing '@' are incorrectly
# split by Qt6ShaderToolsMacros.cmake (it uses '@' as a replacement separator).
function(treeland_add_shaders target resourcename)
    cmake_parse_arguments(arg "BATCHABLE;PRECOMPILE" "PREFIX;BASE" "FILES" ${ARGN})

    set(qsb_outputs "")
    foreach(shader_file IN LISTS arg_FILES)
        get_filename_component(shader_name "${shader_file}" NAME)
        set(qsb_file "${CMAKE_CURRENT_BINARY_DIR}/.qsb/${shader_name}.qsb")
        list(APPEND qsb_outputs "${qsb_file}")

        add_custom_command(
            OUTPUT "${qsb_file}"
            COMMAND Qt6::qsb --qt6 $<$<BOOL:${arg_BATCHABLE}>:--batchable> "${shader_file}" -o "${qsb_file}"
            DEPENDS "${shader_file}" Qt6::qsb
            COMMENT "Compiling shader ${shader_name}"
            VERBATIM
        )
        set_source_files_properties("${qsb_file}" PROPERTIES GENERATED TRUE)
    endforeach()

    add_custom_target(${resourcename}_gen DEPENDS ${qsb_outputs})
    add_dependencies(${target} ${resourcename}_gen)

    qt_add_resources(${target} "${resourcename}"
        PREFIX "${arg_PREFIX}"
        BASE "${arg_BASE}"
        FILES ${qsb_outputs}
    )
endfunction()

