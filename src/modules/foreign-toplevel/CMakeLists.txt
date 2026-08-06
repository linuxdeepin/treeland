find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml
    BASENAME treeland-foreign-toplevel-manager-v1
)

impl_treeland(
    NAME
        module_foreigntoplevel
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/dockpreviewcontextv1.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelhandlev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
