treeland_add_desktop_integration_test(
    NAME desktop_integration_fixture
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/desktop-integration-fixture.c"
)
