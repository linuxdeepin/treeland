find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_wscoplistener main.cpp)

target_link_libraries(test_wscoplistener
    PRIVATE
        Waylib::WaylibServer
        Qt::Core
        Qt::Test
)

add_test(NAME test_wscoplistener COMMAND test_wscoplistener)
