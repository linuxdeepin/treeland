set(MODULE_NAME ext-foreign-toplevel-list)

ws_generate(server wayland-protocols staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml server-protocol)

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

set(MODULE_URI "ExtForeignToplevelList")

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
    extforeigntoplevellist.h
)

target_sources(${MODULE_NAME} PRIVATE
    extforeigntoplevellist.cpp
    server-protocol.c
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
)