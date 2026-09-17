find_package(TreelandProtocols 0.6.0 REQUIRED)

# --- New protocol (treeland-protocols 0.6.0): per-window SSD customization ---
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-decoration-unstable-v1.xml
    BASENAME treeland-decoration-unstable-v1
)

set(PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modules/decoration/decorationmanagerinterfacev1.h
)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/modules/decoration/decorationmanagerinterfacev1.cpp
)

impl_treeland(
    NAME
        module_decoration
    SOURCE
        ${PUBLIC_HEADERS}
        ${SRCS}
    INCLUDE
        $<BUILD_INTERFACE:${WAYLAND_PROTOCOLS_OUTPUTDIR}>
)

if(TREELAND_INSTALL_DEV)
    install(FILES ${PUBLIC_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/treeland/modules/decoration")
endif()
