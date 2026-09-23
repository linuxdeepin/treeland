find_package(TreelandProtocols REQUIRED)

# NOTE: the v2 XML (treeland-dde-shell-unstable-v2.xml) only exists in
# treeland-protocols master / 0.7.0 sources, which are not tagged or
# released yet. The debian build-dependency stays at >= 0.6.0 and the
# build environment must provide the 0.7.0 XML (e.g. the CI workflow
# builds it from the protocols master source).

# --- Deprecated v1 protocol (kept for transition until dde-shell migrates) ---
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-dde-shell-v1.xml
    BASENAME treeland-dde-shell-v1
)

# --- v2 protocol (treeland-protocols 0.7.0; installed flat into the protocols data dir) ---
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
