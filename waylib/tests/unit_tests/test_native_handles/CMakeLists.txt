find_package(Qt6 REQUIRED COMPONENTS Gui Qml Test)

add_executable(test_native_handles main.cpp)

target_link_libraries(test_native_handles
    PRIVATE
        Waylib::WaylibServer
        Qt::Gui
        Qt::Qml
        Qt::Test
)

add_test(NAME test_native_handles COMMAND test_native_handles)

set_property(TEST test_native_handles PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
