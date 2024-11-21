set(MODULE_NAME capture)

find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml treeland-capture-unstable-v1-protocol)

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-capture-v1"
)

target_sources(${MODULE_NAME} PUBLIC
    capture.h
    capture.cpp
    impl/capturev1impl.h
    impl/capturev1impl.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-capture-unstable-v1-protocol.c
)

target_include_directories(${MODULE_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel
    ${CMAKE_SOURCE_DIR}/src/modules/virtual-output
    ${CMAKE_SOURCE_DIR}/src/modules/window-management
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
        item-selector
)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
