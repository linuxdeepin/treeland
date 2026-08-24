include_guard(GLOBAL)

set(TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR "${CMAKE_CURRENT_LIST_DIR}")

pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
set(TREELAND_PROTOCOL_TEST_FRAMEWORK_BINARY_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/framework")
file(MAKE_DIRECTORY "${TREELAND_PROTOCOL_TEST_FRAMEWORK_BINARY_DIR}")
set(xdg_shell_xml "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
set(xdg_shell_header
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_BINARY_DIR}/xdg-shell-client-protocol.h")
set(xdg_shell_code
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_BINARY_DIR}/xdg-shell-client-protocol.c")
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

add_library(treeland_protocol_test_framework STATIC
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/protocol-test-entry.cpp"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/test-accounts-service.cpp"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/test-accounts-service.h"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/client-connection.c"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/test-dconfig-service.cpp"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/test-dconfig-service.h"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/server-bridge.cpp"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/server-bridge-api.h"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/xdg-toplevel-client.c"
    "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}/xdg-toplevel-client.h"
    "${xdg_shell_header}"
    "${xdg_shell_code}"
)
target_include_directories(treeland_protocol_test_framework
    PUBLIC
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_DIR}"
        "${TREELAND_PROTOCOL_TEST_FRAMEWORK_BINARY_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/waylib/src/server"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/kernel"
        "${CMAKE_SOURCE_DIR}/waylib/src/server/utils"
)
target_link_libraries(treeland_protocol_test_framework
    PUBLIC
        libtreeland
        PkgConfig::WAYLAND_CLIENT
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
)
set_target_properties(treeland_protocol_test_framework PROPERTIES C_STANDARD 11)

function(treeland_add_protocol_test)
    set(oneValueArgs NAME XML SETUP CLIENT)
    set(multiValueArgs EXTRA_XMLS EXTRA_LIBRARIES)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    foreach(required NAME SETUP CLIENT)
        if(NOT ARGS_${required})
            message(FATAL_ERROR "treeland_add_protocol_test requires ${required}")
        endif()
    endforeach()

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
        "${ARGS_SETUP}"
        "${ARGS_CLIENT}"
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
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,treeland_protocol_test_framework>"
        ${ARGS_EXTRA_LIBRARIES}
    )
    # Q_OBJECT users live in treeland_protocol_test_framework.  Per-protocol
    # fixtures and C clients do not need an autogen pass of their own.
    set_target_properties(${target} PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        C_STANDARD 11
    )
    add_dependencies(${target} lockscreen multitaskview)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "WLR_BACKENDS=headless;WLR_RENDERER=pixman;DSG_DATA_DIRS=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIRS};TREELAND_PROTOCOL_TEST_DSG_DIR=${TREELAND_PROTOCOL_TEST_DSG_DATA_DIR}"
        LABELS "protocols"
        SKIP_REGULAR_EXPRESSION "SKIP   :"
        SKIP_RETURN_CODE 77
        TIMEOUT 30
    )
endfunction()
