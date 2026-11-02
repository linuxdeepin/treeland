treeland_add_protocol_test(
    NAME session_isolation
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-unstable-v3.xml"
    EXTRA_XMLS
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-prelaunch-splash-unstable-v2.xml"
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-shortcut-manager-unstable-v3.xml"
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-unstable-v1.xml"
        "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-manager-unstable-v1.xml"
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml"
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-layer-shell-unstable-v1.xml"
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-foreign-toplevel-management-unstable-v1.xml"
        "${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/session-isolation.c"
)

if(TARGET test_session_isolation)
    target_sources(test_session_isolation PRIVATE appearance-isolation.c appearance-isolation.h background-window.c background-window.h)
endif()
