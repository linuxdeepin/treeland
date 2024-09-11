set(MODULE_NAME dde-shell)

ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-dde-shell-v1.xml dde-shell-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_library(${MODULE_NAME} STATIC)

target_sources(${MODULE_NAME}
PUBLIC
FILE_SET HEADERS
FILES
    ddeshell.h
    impl/ddeshellv1.h
PRIVATE
    ddeshell.cpp
    impl/ddeshellv1.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/dde-shell-protocol.c
)

target_include_directories(${MODULE_NAME} PUBLIC
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

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-dde-shell-v1.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/treeland-protocols)
