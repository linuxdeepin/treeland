
treeland_add_desktop_integration_test(
    NAME rendered_output_fixture
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/rendered-output-fixture.c"
)
