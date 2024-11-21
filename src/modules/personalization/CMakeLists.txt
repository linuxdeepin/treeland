set(MODULE_NAME personalization)

find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml treeland-personalization-manager-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
find_package(Dtk6Core REQUIRED)

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-personalization-v1"
)

qt_add_resources(${MODULE_NAME} "default_background"
    PREFIX "/"
    FILES
        desktop.webp
)

target_sources(${MODULE_NAME} PUBLIC
FILE_SET HEADERS
FILES
    personalizationmanager.h
    impl/personalization_manager_impl.h
    impl/appearance_impl.h
    impl/types.h
    impl/font_impl.h
)

target_sources(${MODULE_NAME} PRIVATE
    personalizationmanager.cpp
    impl/personalization_manager_impl.cpp
    impl/appearance_impl.cpp
    impl/font_impl.cpp
    ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-personalization-manager-protocol.c
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
        Dtk6::Core
        config
)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
