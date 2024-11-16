set(MODULE_NAME window-management)

find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-window-management-v1.xml treeland-window-management-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_library(${MODULE_NAME} SHARED)

target_sources(${MODULE_NAME} PUBLIC
FILE_SET HEADERS
FILES
    windowmanagement.h
    impl/window_management_impl.h
)

target_sources(${MODULE_NAME} PRIVATE
    windowmanagement.cpp
    impl/window_management_impl.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-window-management-protocol.c
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

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/treeland)