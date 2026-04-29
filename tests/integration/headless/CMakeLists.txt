find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_headless_treeland
    main.cpp
)

target_link_libraries(test_headless_treeland
    PRIVATE
        Qt::Test
)

add_test(
    NAME test_headless_treeland
    COMMAND test_headless_treeland
)

set_property(TEST test_headless_treeland PROPERTY TIMEOUT 20)
set_property(TEST test_headless_treeland PROPERTY
    ENVIRONMENT "TREELAND_TEST_BINARY=$<TARGET_FILE:treeland>"
)
