find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_xdg-dialog
    main.cpp
)

target_link_libraries(test_protocol_xdg-dialog
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_protocol_xdg-dialog COMMAND test_protocol_xdg-dialog)

set_property(TEST test_protocol_xdg-dialog PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_protocol_xdg-dialog PROPERTY
    TIMEOUT 10
)
