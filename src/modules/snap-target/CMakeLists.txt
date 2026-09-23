find_package(TreelandProtocols REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
endif()

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-snap-target-unstable-v1.xml
    BASENAME treeland-snap-target-unstable-v1
)

impl_treeland(
    NAME
        module_snap_target
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snaphandler.h
        ${CMAKE_SOURCE_DIR}/src/modules/snap-target/snaphandler.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
