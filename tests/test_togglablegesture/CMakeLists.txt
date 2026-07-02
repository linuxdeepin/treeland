find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_togglablegesture main.cpp)

target_link_libraries(test_togglablegesture
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_togglablegesture COMMAND test_togglablegesture)

set_property(TEST test_togglablegesture PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_togglablegesture PROPERTY
    TIMEOUT 10
)
