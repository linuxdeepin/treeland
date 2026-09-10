find_package(TreelandProtocols REQUIRED)

# --- Deprecated protocol (kept for backward compatibility) ---
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
    BASENAME treeland-personalization-manager-v1
)

# --- New protocols (treeland-protocols 0.6.0) ---
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-decoration-unstable-v1.xml
    BASENAME treeland-decoration-unstable-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-unstable-v1.xml
    BASENAME treeland-appearance-unstable-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-manager-unstable-v1.xml
    BASENAME treeland-appearance-manager-unstable-v1
)

find_package(Dtk6Core REQUIRED)

set(PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.h
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/decorationmanagerinterfacev1.h
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/appearanceinterfacev1.h
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/appearancemanagerinterfacev1.h
)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/decorationmanagerinterfacev1.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/appearanceinterfacev1.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/appearancemanagerinterfacev1.cpp
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

if(TREELAND_INSTALL_DEV)
    install(FILES ${PUBLIC_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/treeland/modules/personalization")
endif()
