
treeland_add_protocol_test(
    NAME treeland_capture_desktop_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-capture-desktop-v1.c"
)

# CaptureManagerV1 and the production CaptureSourceSelector are supplied by
# the Treeland.Capture module, which libtreeland keeps private.
target_link_libraries(test_treeland_capture_desktop_v1 PRIVATE capture)
target_compile_definitions(test_treeland_capture_desktop_v1 PRIVATE WLR_USE_UNSTABLE)
