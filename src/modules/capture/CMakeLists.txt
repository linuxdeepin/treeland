set(MODULE_NAME capture)

ws_generate_local(server ${CMAKE_CURRENT_LIST_DIR}/protocols/treeland-capture-unstable-v1.xml treeland-capture-unstable-v1-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_library(${MODULE_NAME} STATIC)

target_sources(${MODULE_NAME} PUBLIC
FILE_SET HEADERS
FILES
    capture.h
    impl/capturev1impl.h
)

target_sources(${MODULE_NAME} PRIVATE
    capture.cpp
    impl/capturev1impl.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-capture-unstable-v1-protocol.c
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
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-capture-unstable-v1.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/treeland-protocols)
