find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Test)

add_executable(test_output_frame main.cpp)

target_link_libraries(test_output_frame
    PRIVATE
        Waylib::WaylibServer
        Qt::Gui
        Qt::Qml
        Qt::Quick
        Qt::Test
)

add_test(NAME test_output_frame COMMAND test_output_frame)
set_property(TEST test_output_frame PROPERTY TIMEOUT 30)
