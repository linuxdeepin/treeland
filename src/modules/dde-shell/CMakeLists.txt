find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
    BASENAME treeland-dde-shell-v1
)

impl_treeland(
    NAME
        module_ddeshell
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellattached.h
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev1.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellattached.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
