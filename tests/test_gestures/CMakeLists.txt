find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_gestures main.cpp)

target_link_libraries(test_gestures
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_gestures COMMAND test_gestures)

set_property(TEST test_gestures PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_gestures PROPERTY
    TIMEOUT 10
)
