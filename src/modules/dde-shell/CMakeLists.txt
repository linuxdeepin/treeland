find_package(TreelandProtocols REQUIRED)

# --- Deprecated v1 protocol (kept for transition until dde-shell migrates) ---
# TODO: Remove the v1 protocol and its module sources
# (ddeshellmanagerinterfacev1.*, ddeshellattached.*) in a future release
# once dde-shell has migrated to treeland-dde-shell-unstable-v2.
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
    BASENAME treeland-dde-shell-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-unstable-v2.xml
    BASENAME treeland-dde-shell-unstable-v2
)

impl_treeland(
    NAME
        module_ddeshell
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellattached.h
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev1.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellattached.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev2.h
        ${CMAKE_SOURCE_DIR}/src/modules/dde-shell/ddeshellmanagerinterfacev2.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
