find_package(Qt6 REQUIRED COMPONENTS Gui WaylandClient Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate WaylandClientPrivate)
endif()
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-output-manager)

qt_add_executable(${BIN_NAME}
    main.cpp
    outputuuid.h
    outputuuid.cpp
    virtualclient.h
    virtualclient.cpp
    virtualoutput.h
    virtualoutput.cpp
    virtualoutputmanager.h
    virtualoutputmanager.cpp
    wlroutputmanager.h
    wlroutputmanager.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-unstable-v2.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-unstable-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-head-unstable-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-virtual-output-manager-v1.xml
        ${CMAKE_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
        Qt6::GuiPrivate
        Qt6::WaylandClientPrivate
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
