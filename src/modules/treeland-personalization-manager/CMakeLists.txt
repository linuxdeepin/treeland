set(MODULE_NAME treeland-personalization-manager)

ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/protocols/treeland-personalization-manager-v1.xml server-protocol)
ws_generate(server wayland-protocols stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
find_package(Dtk6Core REQUIRED)

set(MODULE_URI "PersonalizationManager")

qt_add_qml_module(${MODULE_NAME}
    URI TreeLand.Protocols.${MODULE_URI}
    VERSION "1.0"
    STATIC
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Protocols/${MODULE_URI}
)

target_sources(${MODULE_NAME} PUBLIC
FILE_SET HEADERS
FILES
    qwpersonalizationmanager.h
    impl/personalization_manager_impl.h
    impl/personalizationmanager.h
)

target_sources(${MODULE_NAME} PRIVATE
    qwpersonalizationmanager.cpp
    impl/personalization_manager_impl.cpp
    impl/personalizationwindowcontext.cpp
    impl/personalizationwallpapercontext.cpp
    impl/personalizationcursorcontext.cpp
    impl/personalizationmanager.cpp
    server-protocol.c
    xdg-shell-protocol.c
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
        utils
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Quick
        Dtk6::Core
)
