set(MODULE_NAME dde-shell)

find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml dde-shell-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_library(${MODULE_NAME} STATIC)

target_sources(${MODULE_NAME}
PUBLIC
FILE_SET HEADERS
FILES
    ddeshellmanagerv1.h
    impl/ddeshellmanagerv1impl.h
PRIVATE
    ddeshellmanagerv1.cpp
    impl/ddeshellmanagerv1impl.cpp
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
