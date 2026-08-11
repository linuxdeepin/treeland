# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

include_guard(GLOBAL)

set(TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(treeland_add_protocol_test)
    set(oneValueArgs NAME XML SETUP CLIENT)
    set(options XDG_SHELL)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "" ${ARGN})

    foreach(required NAME XML SETUP CLIENT)
        if(NOT ARGS_${required})
            message(FATAL_ERROR "treeland_add_protocol_test requires ${required}")
        endif()
    endforeach()

    string(REPLACE "-" "_" target_suffix "${ARGS_NAME}")
    set(target "test_${target_suffix}")
    get_filename_component(protocol_basename "${ARGS_XML}" NAME_WE)
    set(client_header "${CMAKE_CURRENT_BINARY_DIR}/${protocol_basename}-client-protocol.h")
    set(client_code "${CMAKE_CURRENT_BINARY_DIR}/${protocol_basename}-client-protocol.c")

    add_custom_command(
        OUTPUT "${client_header}"
        COMMAND wayland-scanner client-header "${ARGS_XML}" "${client_header}"
        DEPENDS "${ARGS_XML}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${client_code}"
        COMMAND wayland-scanner private-code "${ARGS_XML}" "${client_code}"
        DEPENDS "${ARGS_XML}" "${client_header}"
        VERBATIM
    )

    set(protocol_test_sources
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-main.cpp"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-client.c"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-server.cpp"
        "${ARGS_SETUP}"
        "${ARGS_CLIENT}"
        "${client_header}"
        "${client_code}"
    )

    if(ARGS_XDG_SHELL)
        pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
        set(xdg_shell_xml "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
        set(xdg_shell_header "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
        set(xdg_shell_code "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.c")
        add_custom_command(
            OUTPUT "${xdg_shell_header}"
            COMMAND wayland-scanner client-header "${xdg_shell_xml}" "${xdg_shell_header}"
            DEPENDS "${xdg_shell_xml}"
            VERBATIM
        )
        add_custom_command(
            OUTPUT "${xdg_shell_code}"
            COMMAND wayland-scanner private-code "${xdg_shell_xml}" "${xdg_shell_code}"
            DEPENDS "${xdg_shell_xml}" "${xdg_shell_header}"
            VERBATIM
        )
        list(APPEND protocol_test_sources
            "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-xdg-client.c"
            "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-xdg-client.h"
            "${xdg_shell_header}"
            "${xdg_shell_code}"
        )
    endif()

    add_executable(${target} ${protocol_test_sources})

    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/waylib/src/server"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/kernel"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/utils"
    )
    target_link_libraries(${target} PRIVATE
        libtreeland
        PkgConfig::WAYLAND_CLIENT
        Qt6::Core
        Qt6::Gui
    )
    set_target_properties(${target} PROPERTIES C_STANDARD 11)

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=headless;WLR_RENDERER=pixman"
        TIMEOUT 30
    )
endfunction()

function(treeland_add_desktop_integration_test)
    set(oneValueArgs NAME XML SETUP CLIENT)
    set(multiValueArgs EXTRA_XMLS EXTRA_LIBRARIES)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    foreach(required NAME SETUP CLIENT)
        if(NOT ARGS_${required})
            message(FATAL_ERROR "treeland_add_desktop_integration_test requires ${required}")
        endif()
    endforeach()

    pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
    set(xdg_shell_xml "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
    set(xdg_shell_header "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
    set(xdg_shell_code "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.c")
    add_custom_command(
        OUTPUT "${xdg_shell_header}"
        COMMAND wayland-scanner client-header "${xdg_shell_xml}" "${xdg_shell_header}"
        DEPENDS "${xdg_shell_xml}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${xdg_shell_code}"
        COMMAND wayland-scanner private-code "${xdg_shell_xml}" "${xdg_shell_code}"
        DEPENDS "${xdg_shell_xml}" "${xdg_shell_header}"
        VERBATIM
    )

    string(REPLACE "-" "_" target_suffix "${ARGS_NAME}")
    set(target "test_${target_suffix}")
    set(protocol_client_sources)
    if(ARGS_XML)
        get_filename_component(protocol_basename "${ARGS_XML}" NAME_WE)
        set(protocol_header "${CMAKE_CURRENT_BINARY_DIR}/${protocol_basename}-client-protocol.h")
        set(protocol_code "${CMAKE_CURRENT_BINARY_DIR}/${protocol_basename}-client-protocol.c")
        add_custom_command(
            OUTPUT "${protocol_header}"
            COMMAND wayland-scanner client-header "${ARGS_XML}" "${protocol_header}"
            DEPENDS "${ARGS_XML}"
            VERBATIM
        )
        add_custom_command(
            OUTPUT "${protocol_code}"
            COMMAND wayland-scanner private-code "${ARGS_XML}" "${protocol_code}"
            DEPENDS "${ARGS_XML}" "${protocol_header}"
            VERBATIM
        )
        list(APPEND protocol_client_sources "${protocol_header}" "${protocol_code}")
    endif()
    foreach(extra_xml IN LISTS ARGS_EXTRA_XMLS)
        get_filename_component(extra_protocol_basename "${extra_xml}" NAME_WE)
        set(extra_protocol_header "${CMAKE_CURRENT_BINARY_DIR}/${extra_protocol_basename}-client-protocol.h")
        set(extra_protocol_code "${CMAKE_CURRENT_BINARY_DIR}/${extra_protocol_basename}-client-protocol.c")
        add_custom_command(
            OUTPUT "${extra_protocol_header}"
            COMMAND wayland-scanner client-header "${extra_xml}" "${extra_protocol_header}"
            DEPENDS "${extra_xml}"
            VERBATIM
        )
        add_custom_command(
            OUTPUT "${extra_protocol_code}"
            COMMAND wayland-scanner private-code "${extra_xml}" "${extra_protocol_code}"
            DEPENDS "${extra_xml}" "${extra_protocol_header}"
            VERBATIM
        )
        list(APPEND protocol_client_sources "${extra_protocol_header}" "${extra_protocol_code}")
    endforeach()
    add_executable(${target}
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-desktop-main.cpp"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-client.c"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-server.cpp"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-xdg-client.c"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-xdg-client.h"
        "${ARGS_SETUP}"
        "${ARGS_CLIENT}"
        "${xdg_shell_header}"
        "${xdg_shell_code}"
        ${protocol_client_sources}
    )
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/waylib/src/server"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/kernel"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/utils"
    )
    target_link_libraries(${target} PRIVATE
        libtreeland
        PkgConfig::WAYLAND_CLIENT
        Qt6::Core
        Qt6::Gui
        ${ARGS_EXTRA_LIBRARIES}
    )
    set_target_properties(${target} PROPERTIES C_STANDARD 11)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=headless;WLR_RENDERER=pixman"
        TIMEOUT 30
    )
endfunction()
