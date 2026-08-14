set(MODULE_NAME snap-target)

find_package(TreelandProtocols REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
endif()

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-snap-target-unstable-v1.xml treeland-snap-target-unstable-v1-protocol)

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-snap-target-v1"
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

qt_add_qml_module(${MODULE_NAME}
    URI Treeland.SnapTarget
    VERSION 1.0
    NO_PLUGIN
    SOURCES
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snaphandler.h
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snaphandler.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snapdetector.h
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snapdetector.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/impl/snapv1impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/impl/snapv1impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-snap-target-unstable-v1-protocol.c
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/SnapTarget
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
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickPrivate
)

impl_treeland(
    NAME
        module_snap_target
    LINK
        ${MODULE_NAME}
)

install(TARGETS ${MODULE_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    NAMELINK_SKIP
)
if(TREELAND_INSTALL_DEV)
    install(TARGETS ${MODULE_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_ONLY
    )
endif()
