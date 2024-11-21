set(MODULE_NAME dde-shell)

find_package(TreelandProtocols REQUIRED)

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-dde-shell-v1"
)

local_qtwayland_server_protocol_treeland(${MODULE_NAME}
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
    BASENAME treeland-dde-shell-v1
)

target_sources(${MODULE_NAME}
PUBLIC
FILE_SET HEADERS
FILES
    ddeshellmanagerinterfacev1.h
    ddeshellattached.h
PRIVATE
    ddeshellmanagerinterfacev1.cpp
    ddeshellattached.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-treeland-dde-shell-v1-server-protocol.c
)

target_include_directories(${MODULE_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

target_link_libraries(${MODULE_NAME}
    PUBLIC
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
