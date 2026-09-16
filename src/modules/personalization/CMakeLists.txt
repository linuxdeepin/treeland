# ============================================================================
# DEPRECATED: treeland-personalization-manager-v1
#
# This module is frozen. The old treeland-personalization-manager-v1 protocol
# has been split into separate protocols in treeland-protocols 0.6.0:
#   - treeland-appearance-unstable-v1        → src/modules/appearance/
#   - treeland-appearance-manager-unstable-v1 → src/modules/appearance/
#   - treeland-decoration-unstable-v1        → (not yet implemented)
#
# No new features will be added here. New code should use the new modules.
# This module is kept only for backward compatibility during the transition.
# ============================================================================

find_package(TreelandProtocols REQUIRED)

# --- Deprecated protocol (kept for backward compatibility) ---
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
    BASENAME treeland-personalization-manager-v1
)

find_package(Dtk6Core REQUIRED)

set(PUBLIC_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.h
)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/modules/personalization/personalizationmanagerinterfacev1.cpp
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
    install(FILES ${CMAKE_SOURCE_DIR}/src/common/windowdecorations.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/treeland/common")
endif()
