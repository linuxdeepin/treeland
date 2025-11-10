set(MODULE_NAME capture)

find_package(TreelandProtocols REQUIRED)
find_package(Qt6 COMPONENTS QuickPrivate QUIET)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml treeland-capture-unstable-v1-protocol)

ws_generate(
    server
    wayland-protocols
    staging/color-management/color-management-v1.xml
    color-management-v1-protocol
)

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-capture-v1"
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

qt_add_qml_module(${MODULE_NAME}
    URI Treeland.Capture
    VERSION 1.0
    NO_PLUGIN
    SOURCES
        ${CMAKE_SOURCE_DIR}/src/modules/capture/capture.h
        ${CMAKE_SOURCE_DIR}/src/modules/capture/capture.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/capture/impl/capturev1impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/capture/impl/capturev1impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-capture-unstable-v1-protocol.c
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Capture
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

target_include_directories(${MODULE_NAME}
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickPrivate
)

impl_treeland(
    NAME
        module_capture
    LINK
        ${MODULE_NAME}
)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
