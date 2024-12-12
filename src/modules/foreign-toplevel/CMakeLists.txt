find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml treeland-foreign-toplevel-manager-protocol)

impl_treeland(
    NAME
        module_foreigntoplevel
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/impl/foreign_toplevel_manager_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/impl/foreign_toplevel_manager_impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-foreign-toplevel-manager-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${WAYLAND_PROTOCOLS_OUTPUTDIR}>
)
