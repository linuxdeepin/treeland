# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

include_guard(GLOBAL)

set(TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(treeland_add_protocol_test)
    set(oneValueArgs NAME XML SETUP CLIENT)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

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

    add_executable(${target}
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-main.cpp"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-client.c"
        "${ARGS_SETUP}"
        "${ARGS_CLIENT}"
        "${client_header}"
        "${client_code}"
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
    )
    set_target_properties(${target} PROPERTIES C_STANDARD 11)

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=headless;WLR_RENDERER=pixman"
        TIMEOUT 30
    )
endfunction()
