find_package(Qt6 REQUIRED COMPONENTS Gui WaylandClient)
find_package(TreelandProtocols 0.7.0 REQUIRED)

set(BIN_NAME test-output-uuid)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-unstable-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-head-unstable-v1.xml
        ${CMAKE_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::WaylandClient
)

# The head protocol creates treeland_output_uuid_v1 objects, but defines neither
# that interface nor treeland_output_uuid_v1_interface. The generated wrapper for
# it is compiled on its own, so it needs the uuid protocol header force-included
# to get the missing declaration.
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-treeland-output-uuid-head-unstable-v1.cpp
    PROPERTIES
        COMPILE_OPTIONS "-include;wayland-treeland-output-uuid-unstable-v1-client-protocol.h"
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
