find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(TreelandProtocols REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

set(PROTOCOL_DIR ${TREELAND_PROTOCOLS_DATA_DIR})

set(GENERATED_SOURCES)
set(GENERATED_HEADERS)

foreach(PROTO wallpaper-color-v1 window-management-v1 virtual-output-manager-v1 prelaunch-splash-v2)
    set(HEADER ${CMAKE_CURRENT_BINARY_DIR}/wayland-treeland-${PROTO}-client-protocol.h)
    set(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/wayland-treeland-${PROTO}.c)

    add_custom_command(
        OUTPUT ${HEADER}
        COMMAND ${WAYLAND_SCANNER} client-header < ${PROTOCOL_DIR}/treeland-${PROTO}.xml > ${HEADER}
        DEPENDS ${PROTOCOL_DIR}/treeland-${PROTO}.xml
        COMMENT "Generating wayland-treeland-${PROTO} client header"
    )

    add_custom_command(
        OUTPUT ${SOURCE}
        COMMAND ${WAYLAND_SCANNER} private-code < ${PROTOCOL_DIR}/treeland-${PROTO}.xml > ${SOURCE}
        DEPENDS ${PROTOCOL_DIR}/treeland-${PROTO}.xml
        COMMENT "Generating wayland-treeland-${PROTO} client code"
    )

    list(APPEND GENERATED_HEADERS ${HEADER})
    list(APPEND GENERATED_SOURCES ${SOURCE})
endforeach()

add_executable(test_protocol_integration
    main.cpp
    ${GENERATED_HEADERS}
    ${GENERATED_SOURCES}
)

target_include_directories(test_protocol_integration
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(test_protocol_integration
    PRIVATE
        libtreeland
        Qt::Test
        PkgConfig::WAYLAND_CLIENT
)

add_test(NAME test_protocol_integration COMMAND test_protocol_integration)

set_property(TEST test_protocol_integration PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_integration PROPERTY
    TIMEOUT 180
)

# This test requires a full Wayland compositor environment with proper
# client-server socket handling. Mark it as "manual" so CI can exclude it.
set_property(TEST test_protocol_integration PROPERTY LABELS "manual")
