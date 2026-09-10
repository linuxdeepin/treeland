treeland_add_protocol_test(
    NAME treeland_virtual_output_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-virtual-output-manager-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-virtual-output-desktop-v1.c"
    EXTRA_XMLS
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-layer-shell-unstable-v1.xml"
        "${PROJECT_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml"
)
