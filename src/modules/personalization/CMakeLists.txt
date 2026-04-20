find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
    BASENAME treeland-personalization-manager-v1
)

find_package(Dtk6Core REQUIRED)

set(PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.h
)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.cpp
    ${CMAKE_BINARY_DIR}/src/modules/personalization/wayland-treeland-personalization-manager-v1-server-protocol.c
)

impl_treeland(
    NAME
        module_personalization
    SOURCE
        ${PUBLIC_HEADERS}
        ${SRCS}
    INCLUDE
        $<BUILD_INTERFACE:${WAYLAND_PROTOCOLS_OUTPUTDIR}>
)

qt_add_resources(libtreeland "default_background"
    PREFIX "/"
    FILES
        desktop.webp
)

install(FILES ${PUBLIC_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/treeland/modules/personalization")
