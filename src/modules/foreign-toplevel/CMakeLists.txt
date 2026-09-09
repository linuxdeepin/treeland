find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-unstable-v2.xml
    BASENAME treeland-foreign-toplevel-manager-unstable-v2
)

impl_treeland(
    NAME
        module_foreigntoplevel
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/dockpreviewcontextv2.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelhandlev2.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv2.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv2.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
