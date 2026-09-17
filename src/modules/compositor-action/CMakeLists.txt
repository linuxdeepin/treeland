find_package(TreelandProtocols 0.7.0 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-compositor-action-unstable-v1.xml
    BASENAME treeland-compositor-action-unstable-v1
)

set(PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modules/compositor-action/compositoractioninterfacev1.h
)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/modules/compositor-action/compositoractioninterfacev1.cpp
)

impl_treeland(
    NAME
        module_compositor_action
    SOURCE
        ${PUBLIC_HEADERS}
        ${SRCS}
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

if(TREELAND_INSTALL_DEV)
    install(FILES ${PUBLIC_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/treeland/modules/compositor-action")
endif()
