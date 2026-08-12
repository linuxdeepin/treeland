find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_wpointer main.cpp)

target_link_libraries(test_wpointer
    PRIVATE
        Waylib::WaylibServer
        Qt::Core
        Qt::Test
)

add_test(NAME test_wpointer COMMAND test_wpointer)
