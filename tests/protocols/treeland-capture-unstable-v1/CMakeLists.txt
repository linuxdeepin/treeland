treeland_add_protocol_test(
    NAME treeland_capture_unstable_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-capture-unstable-v1.c"
)

# CaptureManagerV1 lives in the Treeland.Capture QML module library, which
# libtreeland links privately (impl_treeland), so link it explicitly here.
target_link_libraries(test_treeland_capture_unstable_v1 PRIVATE capture)
target_compile_definitions(test_treeland_capture_unstable_v1 PRIVATE WLR_USE_UNSTABLE)
