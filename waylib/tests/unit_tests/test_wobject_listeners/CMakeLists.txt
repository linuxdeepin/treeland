find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_wobject_listeners main.cpp)

target_link_libraries(test_wobject_listeners
    PRIVATE
        Waylib::WaylibServer
        Qt::Core
        Qt::Test
)

add_test(NAME test_wobject_listeners COMMAND test_wobject_listeners)
