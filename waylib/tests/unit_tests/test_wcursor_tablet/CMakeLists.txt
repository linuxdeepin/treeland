find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_wcursor_tablet main.cpp)

target_link_libraries(test_wcursor_tablet
    PRIVATE
        Waylib::WaylibServer
        Qt::Core
        Qt::Test
)

add_test(NAME test_wcursor_tablet COMMAND test_wcursor_tablet)
